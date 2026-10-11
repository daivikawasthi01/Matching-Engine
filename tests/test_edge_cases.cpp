// Edge cases: market sweeps, empty books, queue positions on cancel,
// partially filled orders, and aggregate level bookkeeping.
#include "test_support.h"

TEST(EdgeCasesTest, MarketSellSweepsBids) {
    OrderBook book;
    book.addOrder(limitOrder(1, Side::Buy, 100, 10));
    book.addOrder(limitOrder(2, Side::Buy, 99, 20));
    book.addOrder(limitOrder(3, Side::Buy, 98, 30));

    auto r = book.addOrder(marketOrder(4, Side::Sell, 25));
    ASSERT_EQ(r.trades.size(), 2u);
    EXPECT_EQ(r.trades[0].buyOrderId, 1u);
    EXPECT_EQ(r.trades[0].price, 100);
    EXPECT_EQ(r.trades[0].quantity, 10);
    EXPECT_EQ(r.trades[1].buyOrderId, 2u);
    EXPECT_EQ(r.trades[1].price, 99);
    EXPECT_EQ(r.trades[1].quantity, 15);

    EXPECT_EQ(book.getOrderCount(), 2u);
    EXPECT_EQ(book.getBestBid(), 99);
    EXPECT_EQ(book.getVolumeAt(Side::Buy, 99), 5);
    EXPECT_EQ(book.getVolumeAt(Side::Buy, 98), 30);
}

TEST(EdgeCasesTest, MarketOrderOnEmptyBookDoesNothing) {
    OrderBook book;
    auto buy = book.addOrder(marketOrder(1, Side::Buy, 10));
    EXPECT_EQ(buy.status, OrderStatus::Accepted);
    EXPECT_TRUE(buy.trades.empty());
    EXPECT_EQ(buy.restingQuantity, 0);
    EXPECT_TRUE(book.empty());

    auto sell = book.addOrder(marketOrder(2, Side::Sell, 10));
    EXPECT_TRUE(sell.trades.empty());
    EXPECT_TRUE(book.empty());
}

TEST(EdgeCasesTest, CancelHeadMiddleAndTailOfQueue) {
    OrderBook book;
    book.addOrder(limitOrder(1, Side::Buy, 100, 10));
    book.addOrder(limitOrder(2, Side::Buy, 100, 20));
    book.addOrder(limitOrder(3, Side::Buy, 100, 30));
    ASSERT_EQ(book.getOrderCount(), 3u);

    // Middle
    EXPECT_TRUE(book.cancelOrder(2));
    EXPECT_EQ(book.getVolumeAt(Side::Buy, 100), 40);

    // Incoming sell of 15 matches order 1 (10) then order 3 (5), skipping order 2
    auto r = book.addOrder(limitOrder(4, Side::Sell, 100, 15));
    ASSERT_EQ(r.trades.size(), 2u);
    EXPECT_EQ(r.trades[0].buyOrderId, 1u);
    EXPECT_EQ(r.trades[0].quantity, 10);
    EXPECT_EQ(r.trades[1].buyOrderId, 3u);
    EXPECT_EQ(r.trades[1].quantity, 5);
    EXPECT_EQ(book.getVolumeAt(Side::Buy, 100), 25);

    // Tail (and now the only order)
    EXPECT_TRUE(book.cancelOrder(3));
    EXPECT_TRUE(book.empty());
    EXPECT_FALSE(book.getBestBid().has_value());
}

TEST(EdgeCasesTest, CancelHeadKeepsRestOfQueue) {
    OrderBook book;
    book.addOrder(limitOrder(1, Side::Sell, 100, 10));
    book.addOrder(limitOrder(2, Side::Sell, 100, 20));
    EXPECT_TRUE(book.cancelOrder(1));
    auto r = book.addOrder(limitOrder(3, Side::Buy, 100, 5));
    ASSERT_EQ(r.trades.size(), 1u);
    EXPECT_EQ(r.trades[0].sellOrderId, 2u);
}

TEST(EdgeCasesTest, PartiallyFilledOrderCanBeCancelled) {
    OrderBook book;
    book.addOrder(limitOrder(1, Side::Sell, 105, 50));
    auto r = book.addOrder(limitOrder(2, Side::Buy, 105, 20));
    ASSERT_EQ(r.trades.size(), 1u);
    EXPECT_EQ(r.trades[0].quantity, 20);
    EXPECT_EQ(book.getVolumeAt(Side::Sell, 105), 30);

    EXPECT_TRUE(book.cancelOrder(1));
    EXPECT_TRUE(book.empty());
    EXPECT_FALSE(book.getBestAsk().has_value());
    EXPECT_EQ(book.getVolumeAt(Side::Sell, 105), 0);
}

TEST(EdgeCasesTest, MultipleLevelsOnBothSides) {
    OrderBook book;
    book.addOrder(limitOrder(1, Side::Buy, 95, 10));
    book.addOrder(limitOrder(2, Side::Buy, 97, 15));
    book.addOrder(limitOrder(3, Side::Buy, 96, 20));
    book.addOrder(limitOrder(4, Side::Sell, 102, 10));
    book.addOrder(limitOrder(5, Side::Sell, 100, 15));
    book.addOrder(limitOrder(6, Side::Sell, 101, 20));

    EXPECT_EQ(book.getOrderCount(), 6u);
    EXPECT_EQ(book.getBestBid(), 97);
    EXPECT_EQ(book.getBestAsk(), 100);
    EXPECT_EQ(book.getLevelCount(Side::Buy), 3u);
    EXPECT_EQ(book.getLevelCount(Side::Sell), 3u);

    auto none = book.addOrder(limitOrder(7, Side::Buy, 99, 5));
    EXPECT_TRUE(none.trades.empty());
    EXPECT_EQ(book.getBestBid(), 99);

    auto sweep = book.addOrder(limitOrder(8, Side::Buy, 101, 30));
    ASSERT_EQ(sweep.trades.size(), 2u);
    EXPECT_EQ(sweep.trades[0].sellOrderId, 5u);
    EXPECT_EQ(sweep.trades[0].price, 100);
    EXPECT_EQ(sweep.trades[0].quantity, 15);
    EXPECT_EQ(sweep.trades[1].sellOrderId, 6u);
    EXPECT_EQ(sweep.trades[1].price, 101);
    EXPECT_EQ(sweep.trades[1].quantity, 15);
    EXPECT_EQ(book.getBestAsk(), 101);
    EXPECT_EQ(book.getVolumeAt(Side::Sell, 101), 5);
}

TEST(EdgeCasesTest, BookIsNeverLeftCrossed) {
    OrderBook book;
    book.addOrder(limitOrder(1, Side::Sell, 100, 10));
    book.addOrder(limitOrder(2, Side::Buy, 105, 25));   // crosses, remainder rests at 105
    ASSERT_TRUE(book.getBestBid().has_value());
    EXPECT_FALSE(book.getBestAsk().has_value());
    EXPECT_EQ(book.getBestBid(), 105);
}

TEST(EdgeCasesTest, VolumeAtTracksFillsAndCancelsExactly) {
    OrderBook book;
    book.addOrder(limitOrder(1, Side::Buy, 100, 7));
    book.addOrder(limitOrder(2, Side::Buy, 100, 11));
    book.addOrder(limitOrder(3, Side::Buy, 100, 13));
    EXPECT_EQ(book.getVolumeAt(Side::Buy, 100), 31);
    EXPECT_EQ(book.getVolumeAt(Side::Sell, 100), 0);
    EXPECT_EQ(book.getVolumeAt(Side::Buy, 101), 0);

    book.addOrder(limitOrder(4, Side::Sell, 100, 9));   // fills 7 + 2
    EXPECT_EQ(book.getVolumeAt(Side::Buy, 100), 22);
    book.cancelOrder(3);
    EXPECT_EQ(book.getVolumeAt(Side::Buy, 100), 9);
}
