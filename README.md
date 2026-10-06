# C++ Matching Engine

A high-performance, deterministic limit order book (LOB) and matching engine core implemented in modern C++17.

The engine simulates core financial exchange infrastructure: enforcing strict price-time priority (FIFO), processing limit and market orders, executing multi-level sweeps and partial fills, rejecting invalid or duplicate orders, and enabling fast $O(\log P)$ order cancellations.

---

## Performance & Benchmark

Benchmarked on **500,000 synthetic orders** (90% Limit Orders across varying price ticks, 10% Market Orders) on an Apple Silicon M-series machine compiled with `-O3`:

| Metric | Result |
| :--- | :--- |
| **Throughput** | **~3,700,000 – 3,900,000 orders/sec** |
| **Average Latency** | **~0.26 µs** (260 ns) |
| **P50 (Median Latency)** | **~0.17 µs** (170 ns) |
| **P90 Latency** | **~0.38 µs** (380 ns) |
| **P99 Latency** | **~1.75 µs** |
| **P99.9 Latency** | **~4.00 µs** |

To run the benchmark on your local machine:
```bash
make run-benchmark
```

---

## Core Architecture & Data Structures

```text
               +-------------------------------------------------------+
               |                       OrderBook                       |
               +-------------------------------------------------------+
                                   /               \
                                  /                 \
                                 v                   v
            +-----------------------+             +-----------------------+
            |      Bids (Map)       |             |      Asks (Map)       |
            | std::greater<double>  |             |  std::less<double>    |
            +-----------------------+             +-----------------------+
            | $101.00 -> [O1, O2]   |             | $102.00 -> [O5, O6]   |
            | $100.50 -> [O3]       |             | $102.50 -> [O7]       |
            | $100.00 -> [O4]       |             | $103.00 -> [O8, O9]   |
            +-----------------------+             +-----------------------+
                        ^                                     ^
                        |                                     |
                        +------------------+------------------+
                                           |
                               +-----------------------+
                               |      orderIndex       |
                               |  std::unordered_map   |
                               |  orderId -> Location  |
                               +-----------------------+
```

1. **Price Levels (`std::map`):**
   - **Bids:** Keyed by descending limit price (`std::greater<double>`), providing $O(1)$ access to the best bid (`bids.begin()`).
   - **Asks:** Keyed by ascending limit price (`std::less<double>`), providing $O(1)$ access to the best ask (`asks.begin()`).
2. **Time Priority (`std::list<Order>`):**
   - Each price level contains a doubly linked list acting as a FIFO queue. Orders arriving earlier at the same price level execute first.
3. **Fast Cancellations & Indexing (`std::unordered_map`):**
   - Maps `orderId -> OrderLocation (side, price, list iterator)`.
   - Cancellation takes $O(\log P)$ time complexity where $P$ is the number of active price levels ($O(1)$ hash map lookup + $O(\log P)$ price level search + $O(1)$ list node erase).

---

## Key Features

- **Price-Time Priority Matching:** Always executes matches at the resting maker's price, prioritizing highest bid / lowest ask, and breaking price ties by earliest arrival time.
- **Limit Orders:** Automatically match if crossed with the opposing side; any unexecuted remainder rests on the book.
- **Market Orders:** Immediate liquidity takers that execute unconditionally against resting liquidity across multiple price tiers. Any unfilled quantity is immediately discarded and never rests on the book.
- **Multi-Level Order Sweeping & Partial Fills:** Large incoming aggressive orders can fill across multiple resting orders and multiple price levels sequentially in a single transaction.
- **Order Cancellation:** Fast $O(\log P)$ removal of active resting orders by unique order ID.
- **Input Validation & Safety:** Rejects duplicate active order IDs and non-positive quantities ($\le 0$).
- **Deterministic Trade Receipts:** Returns detailed trade records including buyer order ID, seller order ID, execution price, and executed quantity.

---

## Project Structure

```text
Matching-Engine/
├── include/
│   ├── order.h             # Order definition, Side (Buy/Sell), and OrderType (Limit/Market)
│   ├── trade.h             # Execution trade receipt structure
│   └── order_book.h        # OrderBook class interface definition
├── src/
│   ├── order_book.cpp      # Matching logic, sweeping, cancellation, and validation
│   └── main.cpp            # Demonstration scenarios
├── tests/
│   ├── test_headers.cpp    # Header self-containment checks
│   ├── test_order_book.cpp # Core unit tests (matching, priority, sweep, validation, cancel)
│   ├── test_edge_cases.cpp # Edge case test suite (market sweeps, empty books, queue position cancels)
│   └── benchmark.cpp       # 500,000-order throughput and latency benchmark
├── Makefile                # Build targets for compilation, tests, and benchmarking
├── CMakeLists.txt          # CMake build configuration
└── README.md               # Project documentation
```

---

## Building and Running

### Prerequisites

- C++17-compatible compiler (`clang++` or `g++`)
- `make` or CMake (3.10+)

### Using Makefile (Recommended)

```bash
# Build everything (demo, test suites, benchmark)
make all

# Run demo walkthrough
./bin/matching_engine

# Run complete test suite (unit tests, edge cases, header checks)
make test

# Run performance benchmark (500,000 orders)
make run-benchmark
```

### Using CMake

```bash
mkdir build && cd build
cmake ..
make
./matching_engine
```

---

## Future Roadmap

- [ ] **Integer / Fixed-Point Ticks:** Transition price representations from `double` to integer tick counts (e.g. `uint64_t`) to eliminate floating-point imprecision.
- [ ] **Lock-Free Ingress Queue:** Single-Producer Single-Consumer (SPSC) ring buffer for low-latency thread-safe order entry.
- [ ] **Binary Protocol Feeds:** Implement lightweight FIX / ITCH message encoders and decoders for network transport.
- [ ] **Persistence & WAL:** Write-ahead logging (WAL) for state recovery across engine restarts.
