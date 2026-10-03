#include "../include/order_book.h"
#include <algorithm>

std::vector<Trade> OrderBook::addOrder(const Order& order) {
    std::vector<Trade> trades;
    Order incoming = order;

    if (incoming.side == Side::Buy) {
        // Buy order matches against resting Sell orders (asks)
        // Price condition: Market orders match unconditionally; Limit orders require incoming price >= best ask
        while (incoming.quantity > 0 && !asks.empty()) {
            auto askIt = asks.begin();
            double bestAskPrice = askIt->first;

            if (incoming.type == OrderType::Limit && incoming.price < bestAskPrice) {
                break; // No price cross
            }

            auto& orderList = askIt->second;
            Order& resting = orderList.front();

            int filledQty = std::min(incoming.quantity, resting.quantity);

            // Record trade: Buyer = incoming, Seller = resting, Price = resting.price (maker price)
            trades.push_back(Trade{incoming.id, resting.id, resting.price, filledQty});

            incoming.quantity -= filledQty;
            resting.quantity -= filledQty;

            // If resting order is fully filled, remove it from list and lookup index
            if (resting.quantity == 0) {
                orderIndex.erase(resting.id);
                orderList.pop_front();
                if (orderList.empty()) {
                    asks.erase(askIt);
                }
            }
        }

        // If limit order has unexecuted volume remaining, rest it on the buy side (bids)
        if (incoming.quantity > 0 && incoming.type == OrderType::Limit) {
            auto& orderList = bids[incoming.price];
            orderList.push_back(incoming);
            auto it = std::prev(orderList.end());
            orderIndex[incoming.id] = OrderLocation{Side::Buy, incoming.price, it};
        }
    } else {
        // Sell order matches against resting Buy orders (bids)
        // Price condition: Market orders match unconditionally; Limit orders require incoming price <= best bid
        while (incoming.quantity > 0 && !bids.empty()) {
            auto bidIt = bids.begin();
            double bestBidPrice = bidIt->first;

            if (incoming.type == OrderType::Limit && incoming.price > bestBidPrice) {
                break; // No price cross
            }

            auto& orderList = bidIt->second;
            Order& resting = orderList.front();

            int filledQty = std::min(incoming.quantity, resting.quantity);

            // Record trade: Buyer = resting, Seller = incoming, Price = resting.price (maker price)
            trades.push_back(Trade{resting.id, incoming.id, resting.price, filledQty});

            incoming.quantity -= filledQty;
            resting.quantity -= filledQty;

            // If resting order is fully filled, remove it from list and lookup index
            if (resting.quantity == 0) {
                orderIndex.erase(resting.id);
                orderList.pop_front();
                if (orderList.empty()) {
                    bids.erase(bidIt);
                }
            }
        }

        // If limit order has unexecuted volume remaining, rest it on the sell side (asks)
        if (incoming.quantity > 0 && incoming.type == OrderType::Limit) {
            auto& orderList = asks[incoming.price];
            orderList.push_back(incoming);
            auto it = std::prev(orderList.end());
            orderIndex[incoming.id] = OrderLocation{Side::Sell, incoming.price, it};
        }
    }

    return trades;
}

bool OrderBook::cancelOrder(int orderId) {
    auto it = orderIndex.find(orderId);
    if (it == orderIndex.end()) {
        return false;
    }

    const OrderLocation& loc = it->second;

    if (loc.side == Side::Buy) {
        auto priceIt = bids.find(loc.price);
        if (priceIt != bids.end()) {
            priceIt->second.erase(loc.it);
            if (priceIt->second.empty()) {
                bids.erase(priceIt);
            }
        }
    } else {
        auto priceIt = asks.find(loc.price);
        if (priceIt != asks.end()) {
            priceIt->second.erase(loc.it);
            if (priceIt->second.empty()) {
                asks.erase(priceIt);
            }
        }
    }

    orderIndex.erase(it);
    return true;
}

bool OrderBook::empty() const {
    return orderIndex.empty();
}

size_t OrderBook::getOrderCount() const {
    return orderIndex.size();
}

std::optional<double> OrderBook::getBestBid() const {
    if (bids.empty()) return std::nullopt;
    return bids.begin()->first;
}

std::optional<double> OrderBook::getBestAsk() const {
    if (asks.empty()) return std::nullopt;
    return asks.begin()->first;
}