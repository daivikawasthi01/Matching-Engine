#include "order_book.h"

#include <algorithm>
#include <iterator>

namespace {

// Does an incoming limit order at `limit` cross a resting order at `restingPrice`?
inline bool crosses(Side incomingSide, Price limit, Price restingPrice) {
    return incomingSide == Side::Buy ? limit >= restingPrice : limit <= restingPrice;
}

}  // namespace

// ---------------------------------------------------------------------------
// Matching helpers
// ---------------------------------------------------------------------------

// Can `incoming` be filled completely against the opposite side right now?
// Walks levels from the best price and sums their cached totals (read-only).
template <typename Book>
bool OrderBook::canFill(const Book& opposite, const Order& incoming) const {
    Quantity available = 0;
    for (const auto& entry : opposite) {
        if (incoming.type == OrderType::Limit &&
            !crosses(incoming.side, incoming.price, entry.first)) {
            break;
        }
        available += entry.second.totalQuantity;
        if (available >= incoming.quantity) {
            return true;
        }
    }
    return false;
}

// Matches `incoming` against the opposite side in price-time priority and
// returns the unfilled remainder. Trades execute at the resting order's price.
template <typename Book>
Quantity OrderBook::sweep(Book& opposite, const Order& incoming, Quantity remaining,
                          std::vector<Trade>& trades) {
    while (remaining > 0 && !opposite.empty()) {
        auto levelIt = opposite.begin();
        if (incoming.type == OrderType::Limit &&
            !crosses(incoming.side, incoming.price, levelIt->first)) {
            break;  // Best opposite price does not cross
        }

        Level& level = levelIt->second;
        while (remaining > 0 && !level.orders.empty()) {
            RestingOrder& resting = level.orders.front();
            const Quantity fill = std::min(remaining, resting.quantity);

            if (incoming.side == Side::Buy) {
                trades.push_back(Trade{incoming.id, resting.id, resting.price, fill, nextSequence_++});
            } else {
                trades.push_back(Trade{resting.id, incoming.id, resting.price, fill, nextSequence_++});
            }

            remaining -= fill;
            resting.quantity -= fill;
            level.totalQuantity -= fill;

            if (resting.quantity == 0) {
                index_.erase(resting.id);
                level.orders.pop_front();
            }
        }

        if (level.orders.empty()) {
            opposite.erase(levelIt);
        }
    }
    return remaining;
}

// Appends an order to the back of its price level (time priority) and indexes it.
template <typename Book>
void OrderBook::rest(Book& book, const Order& order, Quantity quantity, SequenceNumber sequence) {
    Level& level = book[order.price];
    level.orders.push_back(RestingOrder{order.id, order.side, order.price, quantity, sequence});
    level.totalQuantity += quantity;
    index_.emplace(order.id,
                   OrderLocation{order.side, order.price, &level, std::prev(level.orders.end())});
}

// Removes a resting order from its level and the index, erasing the level if
// it becomes empty. `indexIt` is invalidated.
void OrderBook::removeResting(OrderIndex::iterator indexIt) {
    const OrderLocation loc = indexIt->second;
    Level& level = *loc.level;

    level.totalQuantity -= loc.it->quantity;
    level.orders.erase(loc.it);

    if (level.orders.empty()) {
        // `level` dangles after this; it is not used again.
        if (loc.side == Side::Buy) {
            bids_.erase(loc.price);
        } else {
            asks_.erase(loc.price);
        }
    }
    index_.erase(indexIt);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

// Core path for an order that has passed validation.
OrderResult OrderBook::process(const Order& order, std::vector<Trade>& trades) {
    const SequenceNumber orderSequence = nextSequence_++;
    const bool isBuy = order.side == Side::Buy;

    if (order.timeInForce == TimeInForce::FillOrKill) {
        const bool fillable = isBuy ? canFill(asks_, order) : canFill(bids_, order);
        if (!fillable) {
            return OrderResult{OrderStatus::Killed, orderSequence, 0};
        }
    }

    const Quantity remaining = isBuy ? sweep(asks_, order, order.quantity, trades)
                                     : sweep(bids_, order, order.quantity, trades);

    Quantity resting = 0;
    const bool mayRest = order.type == OrderType::Limit &&
                         order.timeInForce == TimeInForce::GoodTillCancel;
    if (mayRest && remaining > 0) {
        if (isBuy) {
            rest(bids_, order, remaining, orderSequence);
        } else {
            rest(asks_, order, remaining, orderSequence);
        }
        resting = remaining;
    }

    return OrderResult{OrderStatus::Accepted, orderSequence, resting};
}

OrderResult OrderBook::addOrder(const Order& order, std::vector<Trade>& trades) {
    trades.clear();

    if (order.quantity <= 0) {
        return OrderResult{OrderStatus::RejectedInvalidQuantity, 0, 0};
    }
    if (order.type == OrderType::Limit && order.price <= 0) {
        return OrderResult{OrderStatus::RejectedInvalidPrice, 0, 0};
    }
    if (index_.find(order.id) != index_.end()) {
        return OrderResult{OrderStatus::RejectedDuplicateId, 0, 0};
    }

    return process(order, trades);
}

AddResult OrderBook::addOrder(const Order& order) {
    AddResult result{OrderStatus::Accepted, 0, 0, {}};
    const OrderResult core = addOrder(order, result.trades);
    result.status = core.status;
    result.sequence = core.sequence;
    result.restingQuantity = core.restingQuantity;
    return result;
}

bool OrderBook::cancelOrder(OrderId id) {
    auto it = index_.find(id);
    if (it == index_.end()) {
        return false;
    }
    removeResting(it);
    ++nextSequence_;
    return true;
}

OrderResult OrderBook::modifyOrder(OrderId id, Price newPrice, Quantity newQuantity,
                                   std::vector<Trade>& trades) {
    trades.clear();

    auto it = index_.find(id);
    if (it == index_.end()) {
        return OrderResult{OrderStatus::RejectedUnknownOrder, 0, 0};
    }
    if (newQuantity <= 0) {
        return OrderResult{OrderStatus::RejectedInvalidQuantity, 0, 0};
    }
    if (newPrice <= 0) {
        return OrderResult{OrderStatus::RejectedInvalidPrice, 0, 0};
    }

    OrderLocation& loc = it->second;

    // Same price and not larger: reduce in place, keeping time priority.
    if (newPrice == loc.price && newQuantity <= loc.it->quantity) {
        loc.level->totalQuantity -= loc.it->quantity - newQuantity;
        loc.it->quantity = newQuantity;
        return OrderResult{OrderStatus::Accepted, nextSequence_++, newQuantity};
    }

    // Otherwise the order loses priority: remove and re-submit.
    const Side side = loc.side;
    removeResting(it);
    return process(Order{id, side, newPrice, newQuantity, OrderType::Limit, TimeInForce::GoodTillCancel},
                   trades);
}

// ---------------------------------------------------------------------------
// Inspection
// ---------------------------------------------------------------------------

bool OrderBook::empty() const {
    return index_.empty();
}

std::size_t OrderBook::getOrderCount() const {
    return index_.size();
}

bool OrderBook::contains(OrderId id) const {
    return index_.find(id) != index_.end();
}

std::optional<Price> OrderBook::getBestBid() const {
    if (bids_.empty()) return std::nullopt;
    return bids_.begin()->first;
}

std::optional<Price> OrderBook::getBestAsk() const {
    if (asks_.empty()) return std::nullopt;
    return asks_.begin()->first;
}

std::size_t OrderBook::getLevelCount(Side side) const {
    return side == Side::Buy ? bids_.size() : asks_.size();
}

Quantity OrderBook::getVolumeAt(Side side, Price price) const {
    if (side == Side::Buy) {
        auto it = bids_.find(price);
        return it == bids_.end() ? 0 : it->second.totalQuantity;
    }
    auto it = asks_.find(price);
    return it == asks_.end() ? 0 : it->second.totalQuantity;
}

SequenceNumber OrderBook::lastSequence() const {
    return nextSequence_ - 1;
}

std::vector<RestingOrder> OrderBook::snapshot(Side side) const {
    std::vector<RestingOrder> out;
    out.reserve(index_.size());
    auto collect = [&out](const auto& book) {
        for (const auto& entry : book) {
            out.insert(out.end(), entry.second.orders.begin(), entry.second.orders.end());
        }
    };
    if (side == Side::Buy) {
        collect(bids_);
    } else {
        collect(asks_);
    }
    return out;
}
