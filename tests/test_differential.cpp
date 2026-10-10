// Randomized differential testing: the real OrderBook and a naive reference
// implementation receive the same random stream of operations. After every
// operation their results, trades, sequence numbers and complete book state
// must be identical, and structural invariants must hold.
#include <cstdint>
#include <map>
#include <random>
#include <set>

#include "reference_book.h"
#include "test_support.h"

namespace {

struct Scenario {
    std::uint64_t seed;
    int operations;
    Price minPrice;
    Price maxPrice;
    const char* name;
};

void PrintTo(const Scenario& s, std::ostream* os) {
    *os << s.name << "(seed=" << s.seed << ")";
}

// Checks invariants that must hold on the real book independent of the reference.
void checkInvariants(const OrderBook& book) {
    const auto bids = book.snapshot(Side::Buy);
    const auto asks = book.snapshot(Side::Sell);

    ASSERT_EQ(book.getOrderCount(), bids.size() + asks.size());
    ASSERT_EQ(book.empty(), bids.empty() && asks.empty());

    // The book must never be left crossed.
    if (!bids.empty() && !asks.empty()) {
        ASSERT_LT(bids.front().price, asks.front().price);
    }
    ASSERT_EQ(book.getBestBid().has_value(), !bids.empty());
    ASSERT_EQ(book.getBestAsk().has_value(), !asks.empty());
    if (!bids.empty()) {
        ASSERT_EQ(*book.getBestBid(), bids.front().price);
    }
    if (!asks.empty()) {
        ASSERT_EQ(*book.getBestAsk(), asks.front().price);
    }

    // Cached level totals and level counts must match the actual orders.
    for (Side side : {Side::Buy, Side::Sell}) {
        const auto& orders = side == Side::Buy ? bids : asks;
        std::map<Price, Quantity> volumes;
        for (const auto& o : orders) {
            ASSERT_GT(o.quantity, 0);
            ASSERT_TRUE(book.contains(o.id));
            volumes[o.price] += o.quantity;
        }
        ASSERT_EQ(book.getLevelCount(side), volumes.size());
        for (const auto& [price, volume] : volumes) {
            ASSERT_EQ(book.getVolumeAt(side, price), volume);
        }
    }
}

}  // namespace

class DifferentialTest : public ::testing::TestWithParam<Scenario> {};

TEST_P(DifferentialTest, MatchesReferenceImplementation) {
    const Scenario s = GetParam();
    std::mt19937_64 rng(s.seed);

    auto uniform = [&rng](std::int64_t lo, std::int64_t hi) {
        return std::uniform_int_distribution<std::int64_t>(lo, hi)(rng);
    };
    auto randomSide = [&] { return uniform(0, 1) == 0 ? Side::Buy : Side::Sell; };
    auto randomPrice = [&] { return uniform(s.minPrice, s.maxPrice); };
    // Mostly passive prices (buys at or below mid, sells above) so books build real
    // depth and queues; the rest are fully random so crossing and sweeps still happen.
    const Price mid = (s.minPrice + s.maxPrice) / 2;
    auto passivePrice = [&](Side side) {
        return side == Side::Buy ? uniform(s.minPrice, mid) : uniform(mid + 1, s.maxPrice);
    };

    OrderBook book;
    ReferenceBook reference;
    std::vector<Trade> bookTrades;
    std::vector<Trade> refTrades;

    // Coverage counters: the stream must actually exercise each path, otherwise
    // agreement with the reference would be vacuous.
    long long tradesSeen = 0, killsSeen = 0, modifiesAccepted = 0, cancelsHit = 0;
    long long duplicatesRejected = 0, ioc = 0, restedWithPriorQueue = 0;

    OrderId nextId = 1;
    std::vector<OrderId> issued;   // every id ever submitted (some no longer resting)
    // Mostly target orders that are really resting (so cancels, modifies and
    // duplicates hit live orders); sometimes stale or never-issued ids.
    auto anIssuedId = [&]() -> OrderId {
        const int pick = static_cast<int>(uniform(0, 9));
        if (pick == 0 || issued.empty()) return nextId + 1000;   // never issued
        if (pick >= 3) {
            const auto orders = book.snapshot(randomSide());
            if (!orders.empty()) {
                return orders[static_cast<std::size_t>(
                                  uniform(0, static_cast<std::int64_t>(orders.size()) - 1))]
                    .id;
            }
        }
        return issued[static_cast<std::size_t>(uniform(0, static_cast<std::int64_t>(issued.size()) - 1))];
    };

    for (int step = 0; step < s.operations; ++step) {
        SCOPED_TRACE(::testing::Message() << "scenario=" << s.name << " seed=" << s.seed
                                          << " step=" << step);
        const int roll = static_cast<int>(uniform(0, 99));

        OrderResult got{};
        OrderResult want{};

        // 0-69 and 78-84: new orders; 70-77: cancels; 85-99: modifies.
        if (roll < 70 || (roll >= 78 && roll < 85)) {
            // ---- new order (various kinds) ----
            const Side side = randomSide();
            const Price price = uniform(0, 9) < 7 ? passivePrice(side) : randomPrice();
            Order order{nextId, side, price, uniform(1, 25)};
            if (roll < 45) {
                // plain GTC limit
            } else if (roll < 52) {
                order.type = OrderType::Market;
                order.price = randomPrice();   // must be ignored
                order.timeInForce = uniform(0, 1) == 0 ? TimeInForce::GoodTillCancel
                                                       : TimeInForce::ImmediateOrCancel;
            } else if (roll < 60) {
                order.timeInForce = TimeInForce::ImmediateOrCancel;
            } else if (roll < 66) {
                order.timeInForce = TimeInForce::FillOrKill;
                order.quantity = uniform(1, 60);
            } else if (roll < 70) {
                order.type = OrderType::Market;
                order.timeInForce = TimeInForce::FillOrKill;
                order.quantity = uniform(1, 60);
            } else {
                // 78..84: deliberately invalid or duplicate orders
                switch (uniform(0, 3)) {
                    case 0: order.quantity = -uniform(0, 5); break;
                    case 1: order.price = -uniform(0, 5); break;
                    case 2: order.id = anIssuedId(); break;
                    default: order.type = OrderType::Market; order.quantity = 0; break;
                }
            }
            const bool queueBefore = book.getVolumeAt(order.side, order.price) > 0;
            got = book.addOrder(order, bookTrades);
            want = reference.addOrder(order, refTrades);
            if (got.status == OrderStatus::Killed) ++killsSeen;
            if (got.status == OrderStatus::RejectedDuplicateId) ++duplicatesRejected;
            if (order.timeInForce == TimeInForce::ImmediateOrCancel &&
                got.status == OrderStatus::Accepted) ++ioc;
            if (queueBefore && got.restingQuantity > 0) ++restedWithPriorQueue;
            issued.push_back(order.id);
            ++nextId;
        } else if (roll < 78) {
            // ---- cancel ----
            const OrderId id = anIssuedId();
            const bool hit = book.cancelOrder(id);
            ASSERT_EQ(hit, reference.cancelOrder(id));
            if (hit) ++cancelsHit;
            bookTrades.clear();
            refTrades.clear();
            got = want = OrderResult{OrderStatus::Accepted, 0, 0};
        } else {
            // ---- modify (85..99), sometimes with invalid values ----
            const OrderId id = anIssuedId();
            Price price = randomPrice();
            Quantity qty = uniform(1, 30);
            if (roll >= 97) {
                if (uniform(0, 1) == 0) qty = -uniform(0, 3); else price = -uniform(0, 3);
            }
            got = book.modifyOrder(id, price, qty, bookTrades);
            want = reference.modifyOrder(id, price, qty, refTrades);
            if (got.status == OrderStatus::Accepted) ++modifiesAccepted;
        }

        tradesSeen += static_cast<long long>(bookTrades.size());
        ASSERT_EQ(got.status, want.status);
        ASSERT_EQ(got.sequence, want.sequence);
        ASSERT_EQ(got.restingQuantity, want.restingQuantity);
        ASSERT_EQ(bookTrades, refTrades);
        ASSERT_EQ(book.lastSequence(), reference.lastSequence());
        ASSERT_EQ(book.snapshot(Side::Buy), reference.snapshot(Side::Buy));
        ASSERT_EQ(book.snapshot(Side::Sell), reference.snapshot(Side::Sell));
        checkInvariants(book);
        if (::testing::Test::HasFatalFailure()) return;
    }

    EXPECT_GT(tradesSeen, 0) << "stream never produced a trade";
    EXPECT_GT(killsSeen, 0) << "stream never killed a FOK order";
    EXPECT_GT(modifiesAccepted, 0) << "stream never accepted a modify";
    EXPECT_GT(cancelsHit, 0) << "stream never cancelled a resting order";
    EXPECT_GT(duplicatesRejected, 0) << "stream never hit a duplicate id";
    EXPECT_GT(ioc, 0) << "stream never ran an IOC order";
    EXPECT_GT(restedWithPriorQueue, 0) << "stream never queued behind an existing order";
}

// Quantity conservation: for every submitted order, traded + resting + discarded
// must equal the submitted quantity (nothing created or destroyed).
TEST(DifferentialConservationTest, QuantityIsConserved) {
    std::mt19937_64 rng(2024);
    auto uniform = [&rng](std::int64_t lo, std::int64_t hi) {
        return std::uniform_int_distribution<std::int64_t>(lo, hi)(rng);
    };

    OrderBook book;
    std::vector<Trade> trades;
    Quantity submitted = 0;
    Quantity traded = 0;   // counted once per trade (a trade consumes quantity on both sides)

    for (OrderId id = 1; id <= 20000; ++id) {
        Order o{id, uniform(0, 1) == 0 ? Side::Buy : Side::Sell, uniform(90, 110), uniform(1, 20)};
        if (uniform(0, 9) == 0) o.type = OrderType::Market;
        submitted += o.quantity;

        const Quantity restingBefore = [&] {
            Quantity q = 0;
            for (Side side : {Side::Buy, Side::Sell}) {
                for (const auto& r : book.snapshot(side)) q += r.quantity;
            }
            return q;
        }();

        const auto result = book.addOrder(o, trades);
        ASSERT_EQ(result.status, OrderStatus::Accepted);
        const Quantity filled = totalQuantity(trades);
        traded += filled;

        Quantity restingAfter = 0;
        for (Side side : {Side::Buy, Side::Sell}) {
            for (const auto& r : book.snapshot(side)) restingAfter += r.quantity;
        }
        // Resting quantity: before + newly rested - consumed on the maker side.
        ASSERT_EQ(restingAfter, restingBefore + result.restingQuantity - filled);
        // The aggressor's quantity is either traded, resting, or (market only) discarded.
        ASSERT_LE(filled + result.restingQuantity, o.quantity);
        if (o.type == OrderType::Limit) {
            ASSERT_EQ(filled + result.restingQuantity, o.quantity);
        }
        if (id % 500 == 0) {
            // Occasionally cancel everything at the best bid to churn the levels.
            auto bids = book.snapshot(Side::Buy);
            for (const auto& r : bids) {
                if (r.price == bids.front().price) book.cancelOrder(r.id);
            }
        }
    }
    EXPECT_GT(traded, 0);
    EXPECT_GT(submitted, traded);
}

static const Scenario kScenarios[] = {
    {1, 4000, 95, 105, "tight"},    {2, 4000, 95, 105, "tight"},    {3, 4000, 95, 105, "tight"},
    {4, 4000, 95, 105, "tight"},    {5, 4000, 98, 102, "very_tight"}, {6, 4000, 98, 102, "very_tight"},
    {7, 4000, 1, 1000, "wide"},     {8, 4000, 1, 1000, "wide"},     {9, 4000, 90, 110, "medium"},
    {10, 4000, 90, 110, "medium"},  {11, 4000, 90, 110, "medium"},  {12, 4000, 99, 101, "two_ticks"},
};

INSTANTIATE_TEST_SUITE_P(Streams, DifferentialTest, ::testing::ValuesIn(kScenarios),
                         [](const ::testing::TestParamInfo<Scenario>& info) {
                             return std::string(info.param.name) + "_seed" +
                                    std::to_string(info.param.seed);
                         });
