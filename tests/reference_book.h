// A deliberately naive reference order book used as a test oracle.
//
// It keeps every resting order in one unsorted vector and answers every
// question with a linear scan or a sort. It shares no data structures or
// algorithms with the real OrderBook, only the written specification
// (price-time priority, maker-price execution, the sequence-number rules and
// the validation order), so agreement between the two is meaningful.
#pragma once

#include <algorithm>
#include <vector>

#include "order_book.h"

class ReferenceBook {
public:
    OrderResult addOrder(const Order& order, std::vector<Trade>& trades) {
        trades.clear();
        if (order.quantity <= 0) return {OrderStatus::RejectedInvalidQuantity, 0, 0};
        if (order.type == OrderType::Limit && order.price <= 0) {
            return {OrderStatus::RejectedInvalidPrice, 0, 0};
        }
        if (find(order.id) != resting_.end()) return {OrderStatus::RejectedDuplicateId, 0, 0};
        return process(order, trades);
    }

    bool cancelOrder(OrderId id) {
        auto it = find(id);
        if (it == resting_.end()) return false;
        resting_.erase(it);
        ++next_;
        return true;
    }

    OrderResult modifyOrder(OrderId id, Price newPrice, Quantity newQuantity,
                            std::vector<Trade>& trades) {
        trades.clear();
        auto it = find(id);
        if (it == resting_.end()) return {OrderStatus::RejectedUnknownOrder, 0, 0};
        if (newQuantity <= 0) return {OrderStatus::RejectedInvalidQuantity, 0, 0};
        if (newPrice <= 0) return {OrderStatus::RejectedInvalidPrice, 0, 0};

        if (newPrice == it->price && newQuantity <= it->quantity) {
            it->quantity = newQuantity;
            return {OrderStatus::Accepted, next_++, newQuantity};
        }
        const Side side = it->side;
        resting_.erase(it);
        return process(Order{id, side, newPrice, newQuantity, OrderType::Limit,
                             TimeInForce::GoodTillCancel},
                       trades);
    }

    std::vector<RestingOrder> snapshot(Side side) const {
        std::vector<RestingOrder> out;
        for (const auto& r : resting_) {
            if (r.side == side) out.push_back(r);
        }
        std::sort(out.begin(), out.end(), [side](const RestingOrder& a, const RestingOrder& b) {
            if (a.price != b.price) {
                return side == Side::Buy ? a.price > b.price : a.price < b.price;
            }
            return a.sequence < b.sequence;
        });
        return out;
    }

    SequenceNumber lastSequence() const { return next_ - 1; }

private:
    using Orders = std::vector<RestingOrder>;

    Orders::iterator find(OrderId id) {
        return std::find_if(resting_.begin(), resting_.end(),
                            [id](const RestingOrder& r) { return r.id == id; });
    }

    static bool crosses(const Order& in, const RestingOrder& r) {
        if (in.type == OrderType::Market) return true;
        return in.side == Side::Buy ? in.price >= r.price : in.price <= r.price;
    }

    // Best crossing opposite order: best price, then earliest sequence.
    Orders::iterator bestMatch(const Order& in) {
        auto best = resting_.end();
        for (auto it = resting_.begin(); it != resting_.end(); ++it) {
            if (it->side == in.side || !crosses(in, *it)) continue;
            if (best == resting_.end()) {
                best = it;
                continue;
            }
            const bool betterPrice =
                in.side == Side::Buy ? it->price < best->price : it->price > best->price;
            if (betterPrice || (it->price == best->price && it->sequence < best->sequence)) {
                best = it;
            }
        }
        return best;
    }

    OrderResult process(const Order& in, std::vector<Trade>& trades) {
        const SequenceNumber seq = next_++;

        if (in.timeInForce == TimeInForce::FillOrKill) {
            Quantity available = 0;
            for (const auto& r : resting_) {
                if (r.side != in.side && crosses(in, r)) available += r.quantity;
            }
            if (available < in.quantity) return {OrderStatus::Killed, seq, 0};
        }

        Quantity remaining = in.quantity;
        while (remaining > 0) {
            auto it = bestMatch(in);
            if (it == resting_.end()) break;
            const Quantity fill = std::min(remaining, it->quantity);
            if (in.side == Side::Buy) {
                trades.push_back(Trade{in.id, it->id, it->price, fill, next_++});
            } else {
                trades.push_back(Trade{it->id, in.id, it->price, fill, next_++});
            }
            remaining -= fill;
            it->quantity -= fill;
            if (it->quantity == 0) resting_.erase(it);
        }

        Quantity restingQty = 0;
        if (in.type == OrderType::Limit && in.timeInForce == TimeInForce::GoodTillCancel &&
            remaining > 0) {
            resting_.push_back(RestingOrder{in.id, in.side, in.price, remaining, seq});
            restingQty = remaining;
        }
        return {OrderStatus::Accepted, seq, restingQty};
    }

    Orders resting_;
    SequenceNumber next_ = 1;
};
