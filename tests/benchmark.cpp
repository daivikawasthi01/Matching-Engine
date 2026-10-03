#include <iostream>
#include <chrono>
#include <vector>
#include <random>
#include <algorithm>
#include <iomanip>
#include "../include/order_book.h"

int main() {
    OrderBook book;
    const int NUM_ORDERS = 500000;

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> side_dist(0, 1);
    std::uniform_real_distribution<double> price_dist(95.0, 105.0);
    std::uniform_int_distribution<int> qty_dist(1, 50);
    std::uniform_int_distribution<int> type_dist(0, 9); // 10% market orders

    std::vector<Order> orders;
    orders.reserve(NUM_ORDERS);
    for (int i = 1; i <= NUM_ORDERS; ++i) {
        OrderType type = (type_dist(rng) == 0) ? OrderType::Market : OrderType::Limit;
        orders.push_back(Order{
            i,
            side_dist(rng) == 0 ? Side::Buy : Side::Sell,
            price_dist(rng),
            qty_dist(rng),
            type
        });
    }

    // Vector to collect individual order processing latencies (in nanoseconds)
    std::vector<long long> latencies_ns;
    latencies_ns.reserve(NUM_ORDERS);

    long long total_trades = 0;
    auto benchmark_start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_ORDERS; ++i) {
        auto t_start = std::chrono::high_resolution_clock::now();
        auto trades = book.addOrder(orders[i]);
        auto t_end = std::chrono::high_resolution_clock::now();

        latencies_ns.push_back(
            std::chrono::duration_cast<std::chrono::nanoseconds>(t_end - t_start).count()
        );
        total_trades += trades.size();
    }

    auto benchmark_end = std::chrono::high_resolution_clock::now();
    double total_time_sec = std::chrono::duration<double>(benchmark_end - benchmark_start).count();

    // Sort latencies for percentile computation
    std::sort(latencies_ns.begin(), latencies_ns.end());

    auto get_percentile = [&](double p) -> double {
        size_t idx = static_cast<size_t>(p * latencies_ns.size());
        if (idx >= latencies_ns.size()) idx = latencies_ns.size() - 1;
        return latencies_ns[idx] / 1000.0; // convert ns to us
    };

    double avg_latency_us = (total_time_sec * 1e6) / NUM_ORDERS;
    double p50_us = get_percentile(0.50);
    double p90_us = get_percentile(0.90);
    double p99_us = get_percentile(0.99);
    double p999_us = get_percentile(0.999);
    double max_us = latencies_ns.back() / 1000.0;

    std::cout << std::fixed << std::setprecision(3);
    std::cout << "========================================\n";
    std::cout << "   Matching Engine Performance Report   \n";
    std::cout << "========================================\n";
    std::cout << "Total Orders Processed : " << NUM_ORDERS << "\n";
    std::cout << "Total Trades Executed  : " << total_trades << "\n";
    std::cout << "Resting Orders in Book : " << book.getOrderCount() << "\n";
    std::cout << "Total Elapsed Time     : " << total_time_sec << " s\n";
    std::cout << "Throughput             : " << static_cast<long long>(NUM_ORDERS / total_time_sec) << " orders/sec\n";
    std::cout << "----------------------------------------\n";
    std::cout << "Latency Metrics (Microseconds):\n";
    std::cout << "  Average Latency      : " << avg_latency_us << " us\n";
    std::cout << "  P50 (Median)         : " << p50_us << " us\n";
    std::cout << "  P90                  : " << p90_us << " us\n";
    std::cout << "  P99                  : " << p99_us << " us\n";
    std::cout << "  P99.9                : " << p999_us << " us\n";
    std::cout << "  Max Latency          : " << max_us << " us\n";
    std::cout << "========================================\n";

    return 0;
}
