// Shared helpers for the test suite.
#pragma once

#include <gtest/gtest.h>

#include <ostream>
#include <vector>

#include "order_book.h"

// Order builders -----------------------------------------------------------

inline Order limitOrder(OrderId id, Side side, Price price, Quantity qty,
                        TimeInForce tif = TimeInForce::GoodTillCancel) {
    return Order{id, side, price, qty, OrderType::Limit, tif};
}

inline Order marketOrder(OrderId id, Side side, Quantity qty,
                         TimeInForce tif = TimeInForce::GoodTillCancel) {
    return Order{id, side, 0, qty, OrderType::Market, tif};
}

// Comparison and printing (so gtest failures are readable) -----------------

inline bool operator==(const RestingOrder& a, const RestingOrder& b) {
    return a.id == b.id && a.side == b.side && a.price == b.price &&
           a.quantity == b.quantity && a.sequence == b.sequence;
}

inline bool operator!=(const RestingOrder& a, const RestingOrder& b) {
    return !(a == b);
}

inline void PrintTo(const Trade& t, std::ostream* os) {
    *os << "Trade{buy=" << t.buyOrderId << ", sell=" << t.sellOrderId
        << ", price=" << t.price << ", qty=" << t.quantity << ", seq=" << t.sequence << "}";
}

inline void PrintTo(const RestingOrder& r, std::ostream* os) {
    *os << "Resting{id=" << r.id << ", " << (r.side == Side::Buy ? "Buy" : "Sell")
        << ", price=" << r.price << ", qty=" << r.quantity << ", seq=" << r.sequence << "}";
}

inline void PrintTo(OrderStatus s, std::ostream* os) {
    *os << toString(s);
}

// Total quantity traded in a list of trades.
inline Quantity totalQuantity(const std::vector<Trade>& trades) {
    Quantity sum = 0;
    for (const auto& t : trades) sum += t.quantity;
    return sum;
}
