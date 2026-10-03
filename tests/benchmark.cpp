#include <iostream>
#include <chrono>
#include <vector>
#include <random>
#include "order_book.h"

int main() {
    OrderBook book;
    const int NUM_ORDERS = 500000;
    
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> side_dist(0, 1);
    std::uniform_real_distribution<double> price_dist(95.0, 105.0);
    std::uniform_int_distribution<int> qty_dist(1, 50);

    std::vector<Order> orders;
    orders.reserve(NUM_ORDERS);
    for (int i = 1; i <= NUM_ORDERS; ++i) {
        orders.push_back(Order{
            i,
            side_dist(rng) == 0 ? Side::Buy : Side::Sell,
            price_dist(rng),
            qty_dist(rng)
        });
    }

    auto start = std::chrono::high_resolution_clock::now();
    
    long long total_trades = 0;
    for (int i = 0; i < NUM_ORDERS; ++i) {
        auto trades = book.addOrder(orders[i]);
        total_trades += trades.size();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end - start;
    
    double elapsed_sec = diff.count();
    double orders_per_sec = NUM_ORDERS / elapsed_sec;
    double avg_latency_us = (elapsed_sec * 1e6) / NUM_ORDERS;

    std::cout << "===== Matching Engine Benchmark =====" << std::endl;
    std::cout << "Total Orders Processed: " << NUM_ORDERS << std::endl;
    std::cout << "Total Trades Executed: " << total_trades << std::endl;
    std::cout << "Elapsed Time: " << elapsed_sec << " s" << std::endl;
    std::cout << "Throughput: " << static_cast<long long>(orders_per_sec) << " orders/sec" << std::endl;
    std::cout << "Avg Latency per Order: " << avg_latency_us << " us" << std::endl;

    return 0;
}
