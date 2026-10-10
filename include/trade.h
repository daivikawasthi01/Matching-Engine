// A trade receipt: produced whenever an incoming order matches a resting order.

#pragma once

#include "order.h"

struct Trade {
    OrderId buyOrderId;        // ID of the buyer
    OrderId sellOrderId;       // ID of the seller
    Price price;               // Execution price in ticks (always the resting order's price)
    Quantity quantity;         // Units exchanged
    SequenceNumber sequence;   // Engine event sequence number of this trade
};

inline bool operator==(const Trade& a, const Trade& b) {
    return a.buyOrderId == b.buyOrderId && a.sellOrderId == b.sellOrderId &&
           a.price == b.price && a.quantity == b.quantity && a.sequence == b.sequence;
}

inline bool operator!=(const Trade& a, const Trade& b) {
    return !(a == b);
}
