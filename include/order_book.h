#pragma once

#include <vector>
#include <map>
#include <list>
#include <unordered_map>
#include <optional>
#include "order.h"
#include "trade.h"

class OrderBook {
public:
    // Adds a new limit or market order. Returns execution trade receipts.
    std::vector<Trade> addOrder(const Order& order);

    // Cancels an active resting order by its ID in O(1) time complexity.
    bool cancelOrder(int orderId);

    // Helper inspection methods
    bool empty() const;
    size_t getOrderCount() const;
    std::optional<double> getBestBid() const;
    std::optional<double> getBestAsk() const;

private:
    // Location tracker for O(1) order cancellation
    struct OrderLocation {
        Side side;
        double price;
        std::list<Order>::iterator it;
    };

    // Bids: Sorted in descending order of price (highest bid first)
    // Each price level contains a FIFO queue (std::list) of orders for time priority
    std::map<double, std::list<Order>, std::greater<double>> bids;

    // Asks: Sorted in ascending order of price (lowest ask first)
    // Each price level contains a FIFO queue (std::list) of orders for time priority
    std::map<double, std::list<Order>, std::less<double>> asks;

    // Hash map index: orderId -> OrderLocation for O(1) cancel lookup
    std::unordered_map<int, OrderLocation> orderIndex;
};