// Core behaviour: matching, price-time priority, sweeps, market orders,
// cancellation, validation, ID reuse and sequence numbers.
#include "test_support.h"

TEST(OrderBookTest, EmptyBookHasNoBestPrices) {
    OrderBook book;
    EXPECT_TRUE(book.empty());
    EXPECT_EQ(book.getOrderCount(), 0u);
    EXPECT_FALSE(book.getBestBid().has_value());
    EXPECT_FALSE(book.getBestAsk().has_value());
    EXPECT_EQ(book.lastSequence(), 0u);
}

TEST(OrderBookTest, BasicMatch) {
    OrderBook book;
    auto r1 = book.addOrder(limitOrder(1, Side::Sell, 100, 10));
    EXPECT_EQ(r1.status, OrderStatus::Accepted);
    EXPECT_TRUE(r1.trades.empty());
    EXPECT_EQ(r1.restingQuantity, 10);
    EXPECT_EQ(book.getOrderCount(), 1u);
    EXPECT_EQ(book.getBestAsk(), 100);

    auto r2 = book.addOrder(limitOrder(2, Side::Buy, 100, 10));
    EXPECT_EQ(r2.status, OrderStatus::Accepted);
    ASSERT_EQ(r2.trades.size(), 1u);
    EXPECT_EQ(r2.trades[0].price, 100);
    EXPECT_EQ(r2.trades[0].quantity, 10);
    EXPECT_EQ(r2.trades[0].buyOrderId, 2u);
    EXPECT_EQ(r2.trades[0].sellOrderId, 1u);
    EXPECT_EQ(r2.restingQuantity, 0);
    EXPECT_TRUE(book.empty());
}

TEST(OrderBookTest, PartialFillRestsRemainder) {
    OrderBook book;
    book.addOrder(limitOrder(1, Side::Sell, 100, 10));
    auto r = book.addOrder(limitOrder(2, Side::Buy, 100, 15));
    ASSERT_EQ(r.trades.size(), 1u);
    EXPECT_EQ(r.trades[0].quantity, 10);
    EXPECT_EQ(r.restingQuantity, 5);
    EXPECT_EQ(book.getOrderCount(), 1u);
    EXPECT_EQ(book.getBestBid(), 100);
    EXPECT_EQ(book.getVolumeAt(Side::Buy, 100), 5);
}

TEST(OrderBookTest, NonCrossingOrdersRest) {
    OrderBook book;
    book.addOrder(limitOrder(1, Side::Buy, 99, 5));
    auto r = book.addOrder(limitOrder(2, Side::Sell, 101, 5));
    EXPECT_TRUE(r.trades.empty());
    EXPECT_EQ(book.getOrderCount(), 2u);
    EXPECT_EQ(book.getBestBid(), 99);
    EXPECT_EQ(book.getBestAsk(), 101);
}

TEST(OrderBookTest, PricePriority) {
    OrderBook book;
    // Insert sell orders in non-sorted price order
    book.addOrder(limitOrder(1, Side::Sell, 105, 10));
    book.addOrder(limitOrder(2, Side::Sell, 101, 10));
    book.addOrder(limitOrder(3, Side::Sell, 103, 10));
    EXPECT_EQ(book.getBestAsk(), 101);

    // 15 units at limit 104 take 10 @ 101 then 5 @ 103
    auto r = book.addOrder(limitOrder(4, Side::Buy, 104, 15));
    ASSERT_EQ(r.trades.size(), 2u);
    EXPECT_EQ(r.trades[0].sellOrderId, 2u);
    EXPECT_EQ(r.trades[0].price, 101);
    EXPECT_EQ(r.trades[0].quantity, 10);
    EXPECT_EQ(r.trades[1].sellOrderId, 3u);
    EXPECT_EQ(r.trades[1].price, 103);
    EXPECT_EQ(r.trades[1].quantity, 5);

    EXPECT_EQ(book.getBestAsk(), 103);
    EXPECT_EQ(book.getVolumeAt(Side::Sell, 103), 5);
}

TEST(OrderBookTest, BidPriceTimePriorityOnSellSide) {
    OrderBook book;
    book.addOrder(limitOrder(1, Side::Buy, 99, 10));
    book.addOrder(limitOrder(2, Side::Buy, 101, 10));
    book.addOrder(limitOrder(3, Side::Buy, 100, 10));
    EXPECT_EQ(book.getBestBid(), 101);

    auto r = book.addOrder(limitOrder(4, Side::Sell, 100, 25));
    ASSERT_EQ(r.trades.size(), 2u);
    EXPECT_EQ(r.trades[0].buyOrderId, 2u);   // 101 first
    EXPECT_EQ(r.trades[1].buyOrderId, 3u);   // then 100
    EXPECT_EQ(r.restingQuantity, 5);         // 99 does not cross a sell at 100
    EXPECT_EQ(book.getBestAsk(), 100);
    EXPECT_EQ(book.getBestBid(), 99);
}

TEST(OrderBookTest, TimePriorityWithinLevel) {
    OrderBook book;
    book.addOrder(limitOrder(1, Side::Buy, 100, 10));
    book.addOrder(limitOrder(2, Side::Buy, 100, 10));

    auto r = book.addOrder(limitOrder(3, Side::Sell, 100, 15));
    ASSERT_EQ(r.trades.size(), 2u);
    EXPECT_EQ(r.trades[0].buyOrderId, 1u);
    EXPECT_EQ(r.trades[0].quantity, 10);
    EXPECT_EQ(r.trades[1].buyOrderId, 2u);
    EXPECT_EQ(r.trades[1].quantity, 5);

    EXPECT_EQ(book.getOrderCount(), 1u);
    EXPECT_EQ(book.getBestBid(), 100);
    EXPECT_EQ(book.getVolumeAt(Side::Buy, 100), 5);
}

TEST(OrderBookTest, MultiLevelSweep) {
    OrderBook book;
    book.addOrder(limitOrder(1, Side::Sell, 100, 10));
    book.addOrder(limitOrder(2, Side::Sell, 101, 20));
    book.addOrder(limitOrder(3, Side::Sell, 102, 30));

    auto r = book.addOrder(limitOrder(4, Side::Buy, 103, 45));
    ASSERT_EQ(r.trades.size(), 3u);
    EXPECT_EQ(r.trades[0].price, 100);
    EXPECT_EQ(r.trades[0].quantity, 10);
    EXPECT_EQ(r.trades[1].price, 101);
    EXPECT_EQ(r.trades[1].quantity, 20);
    EXPECT_EQ(r.trades[2].price, 102);
    EXPECT_EQ(r.trades[2].quantity, 15);

    EXPECT_EQ(book.getOrderCount(), 1u);
    EXPECT_EQ(book.getBestAsk(), 102);
    EXPECT_EQ(book.getLevelCount(Side::Sell), 1u);
}

TEST(OrderBookTest, TradesExecuteAtRestingPrice) {
    OrderBook book;
    book.addOrder(limitOrder(1, Side::Sell, 100, 10));
    auto r = book.addOrder(limitOrder(2, Side::Buy, 110, 10));   // willing to pay more
    ASSERT_EQ(r.trades.size(), 1u);
    EXPECT_EQ(r.trades[0].price, 100);   // maker price, not the aggressor's limit
}

TEST(OrderBookTest, MarketOrdersSweepAndNeverRest) {
    OrderBook book;
    book.addOrder(limitOrder(1, Side::Sell, 100, 10));
    book.addOrder(limitOrder(2, Side::Sell, 102, 10));

    auto r1 = book.addOrder(marketOrder(3, Side::Buy, 15));
    ASSERT_EQ(r1.trades.size(), 2u);
    EXPECT_EQ(r1.trades[0].price, 100);
    EXPECT_EQ(r1.trades[0].quantity, 10);
    EXPECT_EQ(r1.trades[1].price, 102);
    EXPECT_EQ(r1.trades[1].quantity, 5);
    EXPECT_EQ(book.getOrderCount(), 1u);
    EXPECT_EQ(book.getBestAsk(), 102);

    // Only 5 units remain: the market order fills 5 and the rest is discarded.
    auto r2 = book.addOrder(marketOrder(4, Side::Buy, 20));
    ASSERT_EQ(r2.trades.size(), 1u);
    EXPECT_EQ(r2.trades[0].quantity, 5);
    EXPECT_EQ(r2.restingQuantity, 0);
    EXPECT_TRUE(book.empty());
}

TEST(OrderBookTest, MarketOrderIgnoresPriceField) {
    OrderBook book;
    book.addOrder(limitOrder(1, Side::Sell, 100, 10));
    Order o = marketOrder(2, Side::Buy, 10);
    o.price = -5;   // garbage must be ignored for market orders
    auto r = book.addOrder(o);
    EXPECT_EQ(r.status, OrderStatus::Accepted);
    ASSERT_EQ(r.trades.size(), 1u);
    EXPECT_EQ(r.trades[0].price, 100);
}

TEST(OrderBookTest, Cancellation) {
    OrderBook book;
    book.addOrder(limitOrder(1, Side::Buy, 99, 10));
    book.addOrder(limitOrder(2, Side::Buy, 100, 15));
    book.addOrder(limitOrder(3, Side::Sell, 105, 20));

    EXPECT_EQ(book.getOrderCount(), 3u);
    EXPECT_EQ(book.getBestBid(), 100);

    EXPECT_TRUE(book.cancelOrder(2));
    EXPECT_EQ(book.getOrderCount(), 2u);
    EXPECT_EQ(book.getBestBid(), 99);

    EXPECT_FALSE(book.cancelOrder(999));   // never existed
    EXPECT_FALSE(book.cancelOrder(2));     // already cancelled

    EXPECT_TRUE(book.cancelOrder(1));
    EXPECT_TRUE(book.cancelOrder(3));
    EXPECT_TRUE(book.empty());
    EXPECT_FALSE(book.getBestBid().has_value());
    EXPECT_FALSE(book.getBestAsk().has_value());
    EXPECT_EQ(book.getLevelCount(Side::Buy), 0u);
    EXPECT_EQ(book.getLevelCount(Side::Sell), 0u);
}

TEST(OrderBookTest, CancelledOrderCannotBeMatched) {
    OrderBook book;
    book.addOrder(limitOrder(1, Side::Sell, 100, 10));
    ASSERT_TRUE(book.cancelOrder(1));
    auto r = book.addOrder(limitOrder(2, Side::Buy, 100, 10));
    EXPECT_TRUE(r.trades.empty());
    EXPECT_EQ(r.restingQuantity, 10);
}

TEST(OrderBookTest, RejectsNonPositiveQuantity) {
    OrderBook book;
    EXPECT_EQ(book.addOrder(limitOrder(1, Side::Buy, 100, 0)).status,
              OrderStatus::RejectedInvalidQuantity);
    EXPECT_EQ(book.addOrder(limitOrder(2, Side::Sell, 100, -10)).status,
              OrderStatus::RejectedInvalidQuantity);
    EXPECT_EQ(book.addOrder(marketOrder(3, Side::Buy, 0)).status,
              OrderStatus::RejectedInvalidQuantity);
    EXPECT_TRUE(book.empty());
    EXPECT_EQ(book.lastSequence(), 0u);   // rejects consume no sequence numbers
}

TEST(OrderBookTest, RejectsNonPositiveLimitPrice) {
    OrderBook book;
    EXPECT_EQ(book.addOrder(limitOrder(1, Side::Buy, 0, 10)).status,
              OrderStatus::RejectedInvalidPrice);
    EXPECT_EQ(book.addOrder(limitOrder(2, Side::Sell, -3, 10)).status,
              OrderStatus::RejectedInvalidPrice);
    EXPECT_TRUE(book.empty());
}

TEST(OrderBookTest, RejectsDuplicateRestingId) {
    OrderBook book;
    EXPECT_EQ(book.addOrder(limitOrder(1, Side::Buy, 100, 10)).status, OrderStatus::Accepted);

    auto dupBuy = book.addOrder(limitOrder(1, Side::Buy, 105, 20));
    EXPECT_EQ(dupBuy.status, OrderStatus::RejectedDuplicateId);
    EXPECT_TRUE(dupBuy.trades.empty());
    EXPECT_EQ(book.getOrderCount(), 1u);
    EXPECT_EQ(book.getBestBid(), 100);   // original untouched

    // A duplicate must be rejected before it can match anything.
    auto dupSell = book.addOrder(limitOrder(1, Side::Sell, 100, 10));
    EXPECT_EQ(dupSell.status, OrderStatus::RejectedDuplicateId);
    EXPECT_TRUE(dupSell.trades.empty());
    EXPECT_EQ(book.getOrderCount(), 1u);
}

TEST(OrderBookTest, IdIsReusableAfterFullFill) {
    OrderBook book;
    book.addOrder(limitOrder(1, Side::Buy, 100, 10));
    auto fill = book.addOrder(limitOrder(2, Side::Sell, 100, 10));
    ASSERT_EQ(fill.trades.size(), 1u);
    ASSERT_TRUE(book.empty());

    auto reuse = book.addOrder(limitOrder(1, Side::Sell, 102, 5));
    EXPECT_EQ(reuse.status, OrderStatus::Accepted);
    EXPECT_EQ(book.getOrderCount(), 1u);
}

TEST(OrderBookTest, IdIsReusableAfterCancel) {
    OrderBook book;
    book.addOrder(limitOrder(1, Side::Buy, 100, 10));
    ASSERT_TRUE(book.cancelOrder(1));
    EXPECT_EQ(book.addOrder(limitOrder(1, Side::Buy, 100, 10)).status, OrderStatus::Accepted);
}

TEST(OrderBookTest, FullyFilledAggressorIsNotIndexed) {
    OrderBook book;
    book.addOrder(limitOrder(1, Side::Sell, 100, 10));
    book.addOrder(limitOrder(2, Side::Buy, 100, 10));   // fully fills against order 1
    EXPECT_FALSE(book.contains(2));
    EXPECT_FALSE(book.cancelOrder(2));
}

TEST(OrderBookTest, SequenceNumbersAreStrictlyIncreasingAcrossEvents) {
    OrderBook book;
    // 1: sell accepted
    auto r1 = book.addOrder(limitOrder(1, Side::Sell, 100, 10));
    EXPECT_EQ(r1.sequence, 1u);
    // 2: buy accepted, 3: trade
    auto r2 = book.addOrder(limitOrder(2, Side::Buy, 100, 4));
    EXPECT_EQ(r2.sequence, 2u);
    ASSERT_EQ(r2.trades.size(), 1u);
    EXPECT_EQ(r2.trades[0].sequence, 3u);
    // reject consumes nothing
    EXPECT_EQ(book.addOrder(limitOrder(1, Side::Buy, 100, 4)).sequence, 0u);
    EXPECT_EQ(book.lastSequence(), 3u);
    // 4: cancel
    EXPECT_TRUE(book.cancelOrder(1));
    EXPECT_EQ(book.lastSequence(), 4u);
    // failed cancel consumes nothing
    EXPECT_FALSE(book.cancelOrder(1));
    EXPECT_EQ(book.lastSequence(), 4u);
}

TEST(OrderBookTest, SnapshotListsOrdersInPriorityOrder) {
    OrderBook book;
    book.addOrder(limitOrder(1, Side::Buy, 100, 5));
    book.addOrder(limitOrder(2, Side::Buy, 101, 6));
    book.addOrder(limitOrder(3, Side::Buy, 100, 7));
    book.addOrder(limitOrder(4, Side::Sell, 110, 8));
    book.addOrder(limitOrder(5, Side::Sell, 109, 9));

    auto bids = book.snapshot(Side::Buy);
    ASSERT_EQ(bids.size(), 3u);
    EXPECT_EQ(bids[0].id, 2u);   // best price
    EXPECT_EQ(bids[1].id, 1u);   // 100, earlier
    EXPECT_EQ(bids[2].id, 3u);   // 100, later

    auto asks = book.snapshot(Side::Sell);
    ASSERT_EQ(asks.size(), 2u);
    EXPECT_EQ(asks[0].id, 5u);
    EXPECT_EQ(asks[1].id, 4u);
}

TEST(OrderBookTest, ReusedTradeBufferIsClearedEachCall) {
    OrderBook book;
    std::vector<Trade> trades;
    book.addOrder(limitOrder(1, Side::Sell, 100, 10), trades);
    book.addOrder(limitOrder(2, Side::Buy, 100, 10), trades);
    ASSERT_EQ(trades.size(), 1u);
    book.addOrder(limitOrder(3, Side::Buy, 90, 10), trades);   // no match
    EXPECT_TRUE(trades.empty());
}
