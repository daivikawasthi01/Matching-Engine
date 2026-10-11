// Throughput and latency benchmark for the matching engine.
//
// Workload (fixed seed, fully deterministic): integer-tick prices clustered
// around a mid price with an exponentially distributed distance, so most
// orders rest near the touch and a realistic share cross the spread. The mix
// contains GTC limits, market, IOC and FOK orders, cancels (some of which hit
// orders that have already traded, as in real flow) and modifies.
//
// Usage: matching_benchmark [operations=500000] [seed=42] [runs=5]
//
// Always benchmark an optimized build (CMake defaults to Release).

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "order_book.h"

#ifndef MATCHING_BUILD_TYPE
#define MATCHING_BUILD_TYPE "unknown"
#endif

namespace {

using Clock = std::chrono::steady_clock;

enum class OpKind : std::uint8_t { Limit, Market, Ioc, Fok, Cancel, Modify };
constexpr int kKindCount = 6;
const char* const kKindNames[kKindCount] = {"limit (GTC)", "market",  "limit IOC",
                                            "limit FOK",   "cancel",  "modify"};

struct Op {
    OpKind kind;
    Order order;        // for Limit/Market/Ioc/Fok
    OrderId target;     // for Cancel/Modify
    Price newPrice;     // for Modify
    Quantity newQty;    // for Modify
};

constexpr Price kMidPrice = 10000;   // e.g. $100.00 at a $0.01 tick

std::vector<Op> generateWorkload(std::size_t count, std::uint64_t seed) {
    std::mt19937_64 rng(seed);
    std::exponential_distribution<double> offsetDist(1.0 / 6.0);   // mean ~6 ticks
    std::uniform_int_distribution<int> qtyDist(1, 100);
    std::uniform_int_distribution<int> rollDist(0, 99);
    std::uniform_int_distribution<int> sideDist(0, 1);

    auto limitPrice = [&](Side side) {
        const Price offset = static_cast<Price>(offsetDist(rng));
        // Offsets 0 and 1 cross the mid, so a share of orders are aggressive.
        return side == Side::Buy ? kMidPrice + 1 - offset : kMidPrice - 1 + offset;
    };

    std::vector<Op> ops;
    ops.reserve(count);
    std::vector<OrderId> issued;   // ids of GTC limit orders (may no longer be resting)
    OrderId nextId = 1;

    // Real cancel/modify traffic mostly targets recent orders: 80% pick from the
    // latest 5,000 GTC orders, 20% from any (often already traded or cancelled).
    auto pickTarget = [&]() {
        const std::size_t n = issued.size();
        const std::size_t window = std::min<std::size_t>(n, 5000);
        const std::size_t lo = rollDist(rng) < 80 ? n - window : 0;
        return issued[std::uniform_int_distribution<std::size_t>(lo, n - 1)(rng)];
    };

    while (ops.size() < count) {
        const int roll = rollDist(rng);
        const Side side = sideDist(rng) == 0 ? Side::Buy : Side::Sell;
        const bool canTarget = !issued.empty();

        Op op{};
        if (roll >= 70 && canTarget) {
            op.target = pickTarget();
            if (roll < 92) {
                op.kind = OpKind::Cancel;
            } else {
                op.kind = OpKind::Modify;
                op.newPrice = limitPrice(side);
                op.newQty = qtyDist(rng);
            }
        } else if (roll < 58 || roll >= 70) {
            op.kind = OpKind::Limit;
            op.order = Order{nextId, side, limitPrice(side), qtyDist(rng)};
            issued.push_back(nextId++);
        } else if (roll < 63) {
            op.kind = OpKind::Market;
            op.order = Order{nextId++, side, 0, qtyDist(rng), OrderType::Market};
        } else if (roll < 68) {
            op.kind = OpKind::Ioc;
            op.order = Order{nextId++, side, limitPrice(side), qtyDist(rng), OrderType::Limit,
                             TimeInForce::ImmediateOrCancel};
        } else {
            op.kind = OpKind::Fok;
            op.order = Order{nextId++, side, limitPrice(side), qtyDist(rng), OrderType::Limit,
                             TimeInForce::FillOrKill};
        }
        ops.push_back(op);
    }
    return ops;
}

struct Counters {
    long long trades = 0;
    long long checksum = 0;   // keeps the optimizer honest
    long long cancelAttempts = 0;
    long long cancelHits = 0;
    long long modifyAttempts = 0;
    long long modifyHits = 0;
    long long killed = 0;

    void consume(const std::vector<Trade>& t) {
        trades += static_cast<long long>(t.size());
        for (const auto& x : t) checksum += x.price * x.quantity + static_cast<long long>(x.sequence);
    }
};

// Applies one operation using the buffer-reuse API.
inline void apply(OrderBook& book, const Op& op, std::vector<Trade>& buffer, Counters& c) {
    switch (op.kind) {
        case OpKind::Cancel:
            ++c.cancelAttempts;
            if (book.cancelOrder(op.target)) ++c.cancelHits;
            break;
        case OpKind::Modify: {
            ++c.modifyAttempts;
            const OrderResult r = book.modifyOrder(op.target, op.newPrice, op.newQty, buffer);
            if (r.status == OrderStatus::Accepted) ++c.modifyHits;
            c.consume(buffer);
            break;
        }
        default: {
            const OrderResult r = book.addOrder(op.order, buffer);
            if (r.status == OrderStatus::Killed) ++c.killed;
            c.consume(buffer);
            break;
        }
    }
}

// Applies one operation using the convenience API that returns an owning vector.
inline void applyAllocating(OrderBook& book, const Op& op, Counters& c) {
    switch (op.kind) {
        case OpKind::Cancel:
            ++c.cancelAttempts;
            if (book.cancelOrder(op.target)) ++c.cancelHits;
            break;
        case OpKind::Modify: {
            ++c.modifyAttempts;
            std::vector<Trade> local;
            const OrderResult r = book.modifyOrder(op.target, op.newPrice, op.newQty, local);
            if (r.status == OrderStatus::Accepted) ++c.modifyHits;
            c.consume(local);
            break;
        }
        default: {
            const AddResult r = book.addOrder(op.order);
            if (r.status == OrderStatus::Killed) ++c.killed;
            c.consume(r.trades);
            break;
        }
    }
}

double secondsSince(Clock::time_point start) {
    return std::chrono::duration<double>(Clock::now() - start).count();
}

double runBuffered(const std::vector<Op>& ops, Counters& c, std::size_t* restingOut = nullptr) {
    OrderBook book;
    std::vector<Trade> buffer;
    buffer.reserve(64);
    c = Counters{};
    const auto start = Clock::now();
    for (const Op& op : ops) apply(book, op, buffer, c);
    const double s = secondsSince(start);
    if (restingOut) *restingOut = book.getOrderCount();
    return s;
}

double runAllocating(const std::vector<Op>& ops, Counters& c) {
    OrderBook book;
    c = Counters{};
    const auto start = Clock::now();
    for (const Op& op : ops) applyAllocating(book, op, c);
    return secondsSince(start);
}

struct Summary {
    double median, low, high;
};

Summary summarize(std::vector<double> v) {
    std::sort(v.begin(), v.end());
    return {v[v.size() / 2], v.front(), v.back()};
}

long long percentile(const std::vector<long long>& sorted, double p) {
    if (sorted.empty()) return 0;
    std::size_t idx = static_cast<std::size_t>(p * static_cast<double>(sorted.size()));
    if (idx >= sorted.size()) idx = sorted.size() - 1;
    return sorted[idx];
}

long long timerOverheadNs() {
    std::vector<long long> d(200000);
    for (auto& x : d) {
        const auto a = Clock::now();
        const auto b = Clock::now();
        x = std::chrono::duration_cast<std::chrono::nanoseconds>(b - a).count();
    }
    std::sort(d.begin(), d.end());
    return d[d.size() / 2];
}

std::string cpuModel() {
#if defined(__linux__)
    std::ifstream f("/proc/cpuinfo");
    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind("model name", 0) == 0 || line.rfind("Model name", 0) == 0) {
            const auto pos = line.find(':');
            if (pos != std::string::npos) return line.substr(pos + 2);
        }
    }
#elif defined(__APPLE__)
    if (FILE* p = popen("sysctl -n machdep.cpu.brand_string 2>/dev/null", "r")) {
        char buf[256] = {0};
        const bool ok = std::fgets(buf, sizeof(buf), p) != nullptr;
        pclose(p);
        if (ok) {
            std::string s(buf);
            while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
            return s;
        }
    }
#endif
    return "unknown";
}

bool sanitizersEnabled() {
#if defined(__SANITIZE_ADDRESS__)
    return true;
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
    return true;
#else
    return false;
#endif
#else
    return false;
#endif
}

std::string fmtRate(double opsPerSec) {
    std::ostringstream os;
    os << std::fixed << std::setprecision(2) << opsPerSec / 1e6 << "M";
    return os.str();
}

}  // namespace

int main(int argc, char** argv) {
    std::size_t operations = 500000;
    std::uint64_t seed = 42;
    int runs = 5;
    if (argc > 1) operations = static_cast<std::size_t>(std::strtoull(argv[1], nullptr, 10));
    if (argc > 2) seed = std::strtoull(argv[2], nullptr, 10);
    if (argc > 3) runs = std::max(1, std::atoi(argv[3]));
    if (operations == 0) {
        std::cerr << "usage: matching_benchmark [operations] [seed] [runs]\n";
        return 1;
    }

    const std::string buildType = MATCHING_BUILD_TYPE;
    const bool optimized = buildType == "Release" || buildType == "RelWithDebInfo" ||
                           buildType == "MinSizeRel";

    std::cout << "==================== Matching Engine Benchmark ====================\n";
    std::cout << "CPU        : " << cpuModel() << "\n";
#ifdef __VERSION__
    std::cout << "Compiler   : " << __VERSION__ << "\n";
#endif
    std::cout << "Build type : " << buildType
#ifdef NDEBUG
              << " (NDEBUG)"
#endif
              << "\n";
    if (!optimized) {
        std::cout << "WARNING    : not an optimized build; numbers below are not representative\n";
    }
    if (sanitizersEnabled()) {
        std::cout << "WARNING    : sanitizers enabled; numbers below are not representative\n";
    }
    std::cout << "Operations : " << operations << " (seed " << seed << ", " << runs
              << " timed runs after 1 warm-up)\n";

    const std::vector<Op> ops = generateWorkload(operations, seed);

    long long kindCount[kKindCount] = {0};
    for (const Op& op : ops) ++kindCount[static_cast<int>(op.kind)];
    std::cout << "Mix        :";
    for (int k = 0; k < kKindCount; ++k) {
        std::cout << " " << kKindNames[k] << " " << std::fixed << std::setprecision(1)
                  << 100.0 * static_cast<double>(kindCount[k]) / static_cast<double>(operations)
                  << "%";
        if (k + 1 < kKindCount) std::cout << ",";
    }
    std::cout << "\n\n";

    // --- Throughput ---------------------------------------------------------
    Counters counters;
    std::size_t resting = 0;
    runBuffered(ops, counters);   // warm-up (page faults, caches, branch predictors)

    std::vector<double> buffered, allocating;
    for (int i = 0; i < runs; ++i) buffered.push_back(static_cast<double>(operations) / runBuffered(ops, counters, &resting));
    const Counters bufferedCounters = counters;
    for (int i = 0; i < runs; ++i) allocating.push_back(static_cast<double>(operations) / runAllocating(ops, counters));

    const Summary sb = summarize(buffered);
    const Summary sa = summarize(allocating);

    std::cout << "Throughput (operations/sec, median of " << runs << " runs, range in brackets)\n";
    std::cout << "  buffer-reuse API : " << fmtRate(sb.median) << "  [" << fmtRate(sb.low) << " - "
              << fmtRate(sb.high) << "]\n";
    std::cout << "  allocating API   : " << fmtRate(sa.median) << "  [" << fmtRate(sa.low) << " - "
              << fmtRate(sa.high) << "]\n\n";

    std::cout << "Workload outcome: " << bufferedCounters.trades << " trades, " << resting
              << " orders resting at end, cancels hit " << bufferedCounters.cancelHits << "/"
              << bufferedCounters.cancelAttempts << ", modifies accepted "
              << bufferedCounters.modifyHits << "/" << bufferedCounters.modifyAttempts << ", "
              << bufferedCounters.killed << " FOK killed (checksum " << bufferedCounters.checksum
              << ")\n\n";

    // --- Latency ------------------------------------------------------------
    const long long overhead = timerOverheadNs();
    std::vector<long long> all;
    std::vector<long long> perKind[kKindCount];
    all.reserve(operations);

    {
        OrderBook book;
        std::vector<Trade> buffer;
        buffer.reserve(64);
        Counters c;
        for (const Op& op : ops) {
            const auto t0 = Clock::now();
            apply(book, op, buffer, c);
            const auto t1 = Clock::now();
            const long long ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
            all.push_back(ns);
            perKind[static_cast<int>(op.kind)].push_back(ns);
        }
    }

    std::cout << "Latency per operation in nanoseconds (buffer-reuse API; includes ~" << overhead
              << " ns median timer overhead per sample)\n";
    std::cout << std::left << std::setw(14) << "  operation" << std::right << std::setw(10) << "count"
              << std::setw(8) << "p50" << std::setw(8) << "p90" << std::setw(8) << "p99"
              << std::setw(9) << "p99.9" << std::setw(10) << "max" << "\n";

    auto row = [](const char* name, std::vector<long long>& v) {
        std::sort(v.begin(), v.end());
        std::cout << std::left << std::setw(14) << (std::string("  ") + name) << std::right
                  << std::setw(10) << v.size() << std::setw(8) << percentile(v, 0.50)
                  << std::setw(8) << percentile(v, 0.90) << std::setw(8) << percentile(v, 0.99)
                  << std::setw(9) << percentile(v, 0.999) << std::setw(10)
                  << (v.empty() ? 0 : v.back()) << "\n";
    };
    row("all", all);
    for (int k = 0; k < kKindCount; ++k) row(kKindNames[k], perKind[k]);

    std::cout << "====================================================================\n";
    return 0;
}
