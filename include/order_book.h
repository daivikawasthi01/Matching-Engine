#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <list>
#include <map>
#include <optional>
#include <unordered_map>
#include <vector>

#include "order.h"
#include "trade.h"

// Outcome of submitting or modifying an order.
enum class OrderStatus : std::uint8_t {
    Accepted,                // Processed. Check trades / restingQuantity for what happened.
    Killed,                  // Fill-or-kill order that could not be completely filled; nothing executed.
    RejectedInvalidQuantity, // Quantity <= 0
    RejectedInvalidPrice,    // Limit price <= 0
    RejectedDuplicateId,     // An order with this ID is currently resting
    RejectedUnknownOrder     // Modify target is not resting on the book
};

inline const char* toString(OrderStatus status) {
    switch (status) {
        case OrderStatus::Accepted: return "Accepted";
        case OrderStatus::Killed: return "Killed";
        case OrderStatus::RejectedInvalidQuantity: return "RejectedInvalidQuantity";
        case OrderStatus::RejectedInvalidPrice: return "RejectedInvalidPrice";
        case OrderStatus::RejectedDuplicateId: return "RejectedDuplicateId";
        case OrderStatus::RejectedUnknownOrder: return "RejectedUnknownOrder";
    }
    return "Unknown";
}

// Lightweight result; trades are written to a caller-provided buffer.
struct OrderResult {
    OrderStatus status;
    SequenceNumber sequence;   // Engine sequence of this order/modify event; 0 if rejected
    Quantity restingQuantity;  // Quantity left resting on the book after processing
};

// Convenience result that owns its trades (allocates a vector per call).
struct AddResult {
    OrderStatus status;
    SequenceNumber sequence;
    Quantity restingQuantity;
    std::vector<Trade> trades;
};

// A single-threaded limit order book with price-time priority.
//
// Sequence numbers: every accepted (or killed) order, every executed trade,
// every successful cancel and every in-place modify consumes the next value
// of one strictly increasing counter, giving a total order of engine events.
// That makes deterministic replay and write-ahead logging straightforward.
//
// Complexity (P = number of non-empty price levels):
//   addOrder     O(log P) to find or create a level, O(1) per fill,
//                plus O(log P) for each level emptied by the sweep
//   cancelOrder  O(1) hash lookup and list erase; O(log P) only if the
//                cancel empties its price level
//   modifyOrder  O(1) for a same-price quantity reduction, otherwise a
//                cancel followed by addOrder
//   best bid/ask O(1)
class OrderBook {
public:
    // Submits an order. Trades are written to `trades` (cleared first, so a
    // reused vector keeps its capacity and avoids per-call allocation).
    //
    // Rejects: quantity <= 0, limit price <= 0, or an ID that is currently
    // resting. An ID is released as soon as its order is fully filled or
    // cancelled, so it may be reused afterwards.
    OrderResult addOrder(const Order& order, std::vector<Trade>& trades);

    // Convenience overload that returns the trades by value.
    AddResult addOrder(const Order& order);

    // Cancels a resting order by ID. Returns false if it is not resting.
    bool cancelOrder(OrderId id);

    // Changes the price and/or quantity of a resting order.
    // A same-price quantity reduction is done in place and keeps time
    // priority. Any other change loses time priority: the order is removed and
    // re-submitted as a new GoodTillCancel limit order (it may match immediately).
    OrderResult modifyOrder(OrderId id, Price newPrice, Quantity newQuantity,
                            std::vector<Trade>& trades);

    // Inspection
    bool empty() const;
    std::size_t getOrderCount() const;
    bool contains(OrderId id) const;
    std::optional<Price> getBestBid() const;
    std::optional<Price> getBestAsk() const;
    std::size_t getLevelCount(Side side) const;
    Quantity getVolumeAt(Side side, Price price) const;   // 0 if the level does not exist
    SequenceNumber lastSequence() const;                  // 0 before any event

    // All resting orders on one side in priority order (best price first,
    // then earliest arrival). O(n); intended for tests and diagnostics.
    std::vector<RestingOrder> snapshot(Side side) const;

private:
    struct Level {
        std::list<RestingOrder> orders;  // FIFO queue: front has time priority
        Quantity totalQuantity = 0;      // Sum of remaining quantity in `orders`
    };

    using BidMap = std::map<Price, Level, std::greater<Price>>;
    using AskMap = std::map<Price, Level, std::less<Price>>;

    // std::map nodes are stable, so a Level* stays valid until that level is erased.
    struct OrderLocation {
        Side side;
        Price price;
        Level* level;
        std::list<RestingOrder>::iterator it;
    };

    using OrderIndex = std::unordered_map<OrderId, OrderLocation>;

    OrderResult process(const Order& order, std::vector<Trade>& trades);

    template <typename Book>
    bool canFill(const Book& opposite, const Order& incoming) const;

    template <typename Book>
    Quantity sweep(Book& opposite, const Order& incoming, Quantity remaining,
                   std::vector<Trade>& trades);

    template <typename Book>
    void rest(Book& book, const Order& order, Quantity quantity, SequenceNumber sequence);

    void removeResting(OrderIndex::iterator indexIt);

    BidMap bids_;   // Highest price first
    AskMap asks_;   // Lowest price first
    OrderIndex index_;
    SequenceNumber nextSequence_ = 1;
};
