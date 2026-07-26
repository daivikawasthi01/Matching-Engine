# C++ Matching Engine

A lightweight, deterministic order book and matching engine core built in C++17.

This project simulates the core infrastructure of a financial exchange: processing limit orders, executing trades based on price-time priority, handling partial fills, and managing order cancellations.

---

## How It Works

At the core of an exchange, a matching engine maintains two sets of orders inside an order book: **bids** (buy orders) and **asks** (sell orders).

When a new order enters the book, the engine checks for a price cross:
- **Incoming Buy Order:** Matches if its limit price is greater than or equal to the lowest ask price currently on the book.
- **Incoming Sell Order:** Matches if its limit price is less than or equal to the highest bid price currently on the book.

If the incoming order's quantity exceeds the available resting quantity, the engine sweeps through multiple resting orders until the incoming quantity is exhausted or no matching prices remain. Any unexecuted remainder rests on the book as a new order.

---

## Key Features

- **Limit Order Processing** — instant trade execution when prices cross; unmatched volume rests on the book.
- **Order Sweeping & Partial Fills** — a single large incoming order can fill against multiple smaller resting orders sequentially.
- **Order Cancellation** — removes active or partially filled resting orders by order ID.
- **Execution Receipts** — returns detailed trade records containing buyer and seller IDs, execution price, and filled quantity.

---

## Project Structure

```text
Matching-Engine/
├── include/
│   ├── order.h         # Order entity and Side enum (Buy/Sell)
│   ├── trade.h         # Execution trade receipt structure
│   └── order_book.h    # OrderBook class interface definition
├── src/
│   ├── order_book.cpp  # Matching engine logic, sweeping, and cancellation
│   └── main.cpp        # Driver script and test scenarios
├── CMakeLists.txt      # Build configuration
└── README.md
```

---

## Example Flow

1. **Order 1 (Sell):** 10 units at $100.00. No buyers available — Order 1 rests on the sell side.
2. **Order 2 (Buy):** 15 units at $100.00. Prices cross ($100.00 ≥ $100.00).
   - Trades 10 units against Order 1.
   - Order 1 is fully filled and removed.
   - The remaining 5 units of Order 2 rest on the buy side.
3. **Cancel Request:** Cancel Order 2 — the remaining 5 resting units are removed from the book.

---

## Building and Running

**Prerequisites**
- C++17-compatible compiler (`g++` or `clang++`)
- CMake 3.10 or higher

**Build Instructions**
```bash
git clone https://github.com/daivikawasthi/Matching-Engine.git
cd Matching-Engine

mkdir build && cd build
cmake ..
make

./matching_engine
```

---

## Current Limitations

This is a correctness-first implementation (v1). Notably:
- Resting orders are stored in plain `std::vector`s, not price-indexed structures — matching against the "best" price currently relies on insertion order rather than true price-time priority.
- No market orders, order types beyond limit, or timestamp-based tie-breaking yet.
- Single-threaded, no networking/protocol layer, no persistence.

## Roadmap

- [ ] **Price-time priority** — replace vectors with a sorted price-level structure (`std::map<price, deque<Order>>`) plus a hash map for O(1) cancel-by-ID.
- [ ] **Market orders** — execute immediately against best available liquidity, no limit price.
- [ ] **Automated test suite** — move ad hoc scenarios out of `main.cpp` into a proper test framework (GoogleTest/Catch2).
- [ ] **Concurrency** — lock-free structures for concurrent order submission.
- [ ] **Benchmarking** — microsecond-level latency measurements for order entry and matching.
