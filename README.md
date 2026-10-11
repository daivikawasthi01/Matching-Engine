# C++ Matching Engine

[![CI](https://github.com/daivikawasthi01/Matching-Engine/actions/workflows/ci.yml/badge.svg)](https://github.com/daivikawasthi01/Matching-Engine/actions/workflows/ci.yml)

A deterministic limit order book and matching engine core in modern C++17: price-time priority, limit and market orders, IOC / FOK, modify, cancel-by-ID, integer-tick prices, and a strictly increasing sequence number on every engine event.

It is a single-threaded engine core (no network layer or persistence yet), built to be correct first and then measured honestly.

---

## Quick Start

```bash
make test            # build (Release) and run all 60+ tests
make sanitize        # same tests under AddressSanitizer + UBSan
make run-benchmark   # throughput and latency benchmark
make demo            # walkthrough of the main features
```

`make` is a thin wrapper around CMake (3.14+, any C++17 compiler). GoogleTest is fetched automatically if it is not installed. The default build type is `Release`.

```cpp
#include "order_book.h"

OrderBook book;
book.addOrder(Order{1, Side::Sell, 10100, 10});              // rests: sell 10 @ 101.00 (ticks of $0.01)

AddResult r = book.addOrder(Order{2, Side::Buy, 10100, 4});
// r.status == OrderStatus::Accepted
// r.trades[0]: buyer 2, seller 1, price 10100 (maker price), qty 4, sequence 3

// Hot path: reuse one trade buffer to avoid a heap allocation per call.
std::vector<Trade> trades;
OrderResult res = book.addOrder(Order{3, Side::Buy, 10100, 6, OrderType::Limit,
                                      TimeInForce::ImmediateOrCancel}, trades);
```

---

## Features

- **Price-time priority.** Best price first; earliest arrival first within a price. Trades always execute at the resting (maker) order's price.
- **Limit and market orders.** Market orders sweep any number of price levels and never rest.
- **Time in force.** `GoodTillCancel` (default), `ImmediateOrCancel` (discard the unfilled remainder) and `FillOrKill` (execute completely or not at all; a killed order changes nothing on the book).
- **Modify.** Reducing quantity at the same price is done in place and keeps time priority. Any other change loses priority (cancel and re-submit), and can match immediately if the new price crosses.
- **Cancel by ID**, including orders that are partially filled.
- **Integer ticks.** Prices are `int64` tick counts, never floating point.
- **Explicit results.** Every call reports an `OrderStatus` (`Accepted`, `Killed`, `RejectedInvalidQuantity`, `RejectedInvalidPrice`, `RejectedDuplicateId`, `RejectedUnknownOrder`), so a rejection is never confused with "no trades".
- **Sequence numbers.** One strictly increasing counter is consumed by every accepted or killed order, trade, successful cancel and in-place modify. That gives a total order of events and makes deterministic replay and write-ahead logging straightforward.

---

## Design

```text
               +-------------------------------------------------------+
               |                       OrderBook                       |
               +-------------------------------------------------------+
                         /                  |                  \
                        v                   v                   v
      +----------------------+   +----------------------+   +----------------------+
      |     bids_ (map)      |   |      asks_ (map)     |   |   index_ (hash map)  |
      |  Price -> Level      |   |  Price -> Level      |   | OrderId -> Location  |
      |  std::greater<Price> |   |  std::less<Price>    |   | {side, price,        |
      +----------------------+   +----------------------+   |  Level*, list::iter} |
      | 10100 -> Level       |   | 10200 -> Level       |   +----------------------+
      |  orders: [O1, O2]    |   |  orders: [O5, O6]    |
      |  totalQuantity: 17   |   |  totalQuantity: 30   |
      | 10050 -> Level       |   | 10250 -> Level       |
      |  orders: [O3]        |   |  orders: [O7]        |
      +----------------------+   +----------------------+
```

- **Price levels:** `std::map<Price, Level>` per side, ordered so `begin()` is always the best price (O(1) best bid/ask).
- **Time priority:** each `Level` holds a FIFO `std::list` of resting orders plus a cached `totalQuantity`.
- **Fast cancel:** `index_` maps each order ID to its level pointer and list iterator. Map nodes are stable, so the pointer stays valid until the level is erased.
- **FOK without side effects:** feasibility is checked read-only over the cached level totals before anything is touched.

| Operation | Cost (P = non-empty price levels) |
| :--- | :--- |
| `addOrder` | O(log P) to find or create the level, O(1) per fill, plus O(log P) per level emptied |
| `cancelOrder` | O(1) hash lookup and list erase; O(log P) only when the cancel empties its level |
| `modifyOrder` | O(1) for an in-place reduction; otherwise a cancel plus `addOrder` |
| Best bid / ask | O(1) |
| `getVolumeAt` | O(log P) |
| FOK feasibility check | O(levels crossed) |

**Rules worth knowing**

- Validation order: quantity must be > 0, then limit price must be > 0 (market orders ignore the price field), then the ID must not currently be resting.
- An ID is released as soon as its order is fully filled or cancelled and may then be reused. Duplicate detection covers resting orders only.
- Market orders and IOC/FOK orders never rest. A GTC market order behaves like IOC.
- A rejected request consumes no sequence number; a killed FOK order does.

---

## Testing

`make test` runs 60+ GoogleTest cases through CTest:

- **Unit tests** for matching, price-time priority, sweeps, market orders, cancels, validation, ID reuse, IOC, FOK, modify and sequence numbers.
- **Differential tests.** A deliberately naive reference book (one unsorted vector, linear scans) shares no code or data structures with the real engine. Twelve seeded random streams (mixed GTC, market, IOC, FOK, cancels, modifies, duplicates and invalid input, over tight and wide price ranges) are fed to both, and after every operation the results, trades, sequence numbers and full book state must be identical. Each stream also asserts that it actually exercised trades, kills, modifies, cancels and duplicate rejections, so agreement cannot be vacuous.
- **Invariants** checked after every step: the book is never crossed, cached level totals match the orders, the index matches the book.
- **Header self-containment:** every public header is compiled on its own as part of the build.
- **Sanitizers:** `make sanitize` runs everything under AddressSanitizer and UndefinedBehaviorSanitizer. CI runs it on every push.

Tests use GoogleTest rather than `assert`, so they still run in `Release` builds where `NDEBUG` would silently disable `assert`.

---

## Benchmark

```bash
make run-benchmark
# or: ./build/matching_benchmark [operations=500000] [seed=42] [runs=5]
```

The workload is deterministic (fixed seed): integer-tick prices clustered around a mid price with an exponentially distributed distance, so most orders rest near the touch and a share cross the spread. The mix is 58% GTC limit, 5% market, 5% IOC, 2% FOK, 22% cancel and 8% modify. Cancels and modifies mostly target recent orders, and some hit orders that have already traded, as in real flow. The benchmark prints the CPU, compiler, build type and workload outcome, warns if the build is not optimized, runs one warm-up pass, and reports the median of several timed runs.

**Sample results**: Intel Xeon @ 2.8 GHz (shared 2-vCPU cloud VM), GCC 13.3, `Release`, 500,000 operations. Two separate invocations; ranges show run-to-run variation on a noisy VM, so expect different numbers on your machine.

| Metric | Result |
| :--- | :--- |
| Throughput, buffer-reuse API | ~4.8 – 4.9 M operations/sec (median of 15 runs) |
| Throughput, allocating API | ~3.8 – 4.0 M operations/sec |
| Latency p50 (all operations) | ~170 – 220 ns |
| Latency p99 | ~1.0 – 1.1 µs |
| Latency p99.9 | ~2 – 4 µs |

Latency samples include roughly 20 ns of timer overhead each. The maximum latency (single-digit milliseconds) is OS scheduling noise on the shared VM, not engine behaviour. The buffer-reuse overload (`addOrder(order, trades)`) avoids a `std::vector` allocation per call and was measurably faster than the convenience overload that returns an `AddResult`.

---

## Limitations

- Single-threaded; no network protocol, market-data feed or persistence yet.
- No self-trade prevention (orders carry no participant ID).
- One heap-allocated list node per resting order, and a `std::map` for price levels. A pooled allocator or a flat price ladder indexed by tick would cut latency further.
- Quantities are `int64` with no overflow checks on aggregate sums.
- Duplicate-ID detection covers currently resting orders only.

## Roadmap

- [ ] **Write-ahead log and deterministic replay.** Sequence numbers are already in place; log accepted events and rebuild the book from the log.
- [ ] **Self-trade prevention** with a participant ID on orders.
- [ ] **Lock-free ingress queue.** Single-producer single-consumer ring buffer for thread-safe order entry.
- [ ] **Network gateway and market-data feed.** Binary protocol (FIX / ITCH-style) encoders and decoders, a TCP or WebSocket gateway, and a depth/trade feed.
- [ ] **Allocator and price-ladder optimizations**, profiled before and after.

---

## Project Structure

```text
Matching-Engine/
├── include/
│   ├── order.h              # Value types, Side, OrderType, TimeInForce, Order, RestingOrder
│   ├── trade.h              # Trade receipt (with engine sequence number)
│   └── order_book.h         # OrderBook API, OrderStatus, OrderResult, AddResult
├── src/
│   ├── order_book.cpp       # Matching, cancel, modify, FOK check, inspection
│   └── main.cpp             # Demo walkthrough
├── tests/
│   ├── test_support.h       # Order builders and printers shared by the tests
│   ├── test_order_book.cpp  # Core matching, priority, validation, sequences
│   ├── test_edge_cases.cpp  # Market sweeps, queue positions, level bookkeeping
│   ├── test_time_in_force.cpp  # IOC and FOK
│   ├── test_modify.cpp      # Modify semantics and priority rules
│   ├── reference_book.h     # Naive reference implementation (test oracle)
│   └── test_differential.cpp   # Randomized comparison against the reference
├── benchmarks/
│   └── benchmark.cpp        # Throughput and latency benchmark
├── .github/workflows/ci.yml # GCC, Clang, macOS and sanitizer builds
├── CMakeLists.txt           # Build, tests (CTest), benchmark, sanitizer option
├── Makefile                 # Convenience wrapper around CMake
└── README.md
```
