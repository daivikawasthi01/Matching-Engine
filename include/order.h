#pragma once

#include <cstdint>

// Fundamental value types.
//
// Prices are integers counted in ticks (e.g. with a tick size of $0.01,
// a price of 10050 means $100.50). Floating-point prices are deliberately
// avoided: they make equality, hashing and ordering of price levels unsafe.
using OrderId = std::uint64_t;
using Price = std::int64_t;
using Quantity = std::int64_t;
using SequenceNumber = std::uint64_t;

// Side of the order in the book.
enum class Side : std::uint8_t {
    Buy,
    Sell
};

// Limit: executes at the limit price or better, remainder may rest on the book.
// Market: executes immediately against available liquidity at any price and
//         never rests on the book (the price field is ignored).
enum class OrderType : std::uint8_t {
    Limit,
    Market
};

// How long an order stays active.
//   GoodTillCancel:    unfilled limit remainder rests until cancelled.
//   ImmediateOrCancel: execute what is possible now, discard the remainder.
//   FillOrKill:        execute completely now or not at all.
// Market orders never rest, so GoodTillCancel behaves like ImmediateOrCancel
// for them.
enum class TimeInForce : std::uint8_t {
    GoodTillCancel,
    ImmediateOrCancel,
    FillOrKill
};

struct Order {
    OrderId id;                // Unique among orders currently resting on the book
    Side side;
    Price price;               // Limit price in ticks, must be > 0 (ignored for Market)
    Quantity quantity;         // Must be > 0
    OrderType type = OrderType::Limit;
    TimeInForce timeInForce = TimeInForce::GoodTillCancel;
};

// A limit order resting on the book, as reported by OrderBook::snapshot().
struct RestingOrder {
    OrderId id;
    Side side;
    Price price;
    Quantity quantity;         // Remaining (unfilled) quantity
    SequenceNumber sequence;   // Arrival sequence; defines time priority within a price level
};
