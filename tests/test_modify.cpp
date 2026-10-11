// modifyOrder: in-place reductions keep priority, everything else loses it.
#include "test_support.h"

TEST(ModifyTest, ReducingQuantityKeepsTimePriority) {
    OrderBook book;
    book.addOrder(limitOrder(1, Side::Buy, 100, 10));
    book.addOrder(limitOrder(2, Side::Buy, 100, 10));

    std::vector<Trade> trades;
    auto m = book.modifyOrder(1, 100, 4, trades);
    EXPECT_EQ(m.status, OrderStatus::Accepted);
    EXPECT_EQ(m.restingQuantity, 4);
    EXPECT_TRUE(trades.empty());
    EXPECT_EQ(book.getVolumeAt(Side::Buy, 100), 14);

    auto snap = book.snapshot(Side::Buy);
    ASSERT_EQ(snap.size(), 2u);
    EXPECT_EQ(snap[0].id, 1u);   // still first in the queue
    EXPECT_EQ(snap[0].quantity, 4);

    // And it is the one that trades first.
    auto r = book.addOrder(limitOrder(3, Side::Sell, 100, 5));
    ASSERT_EQ(r.trades.size(), 2u);
    EXPECT_EQ(r.trades[0].buyOrderId, 1u);
    EXPECT_EQ(r.trades[0].quantity, 4);
    EXPECT_EQ(r.trades[1].buyOrderId, 2u);
    EXPECT_EQ(r.trades[1].quantity, 1);
}

TEST(ModifyTest, IncreasingQuantityLosesTimePriority) {
    OrderBook book;
    book.addOrder(limitOrder(1, Side::Buy, 100, 10));
    book.addOrder(limitOrder(2, Side::Buy, 100, 10));

    std::vector<Trade> trades;
    auto m = book.modifyOrder(1, 100, 15, trades);
    EXPECT_EQ(m.status, OrderStatus::Accepted);
    EXPECT_EQ(m.restingQuantity, 15);

    auto snap = book.snapshot(Side::Buy);
    ASSERT_EQ(snap.size(), 2u);
    EXPECT_EQ(snap[0].id, 2u);   // order 2 now has priority
    EXPECT_EQ(snap[1].id, 1u);
    EXPECT_EQ(book.getVolumeAt(Side::Buy, 100), 25);
}

TEST(ModifyTest, SamePriceAndQuantityIsANoOpThatKeepsPriority) {
    OrderBook book;
    book.addOrder(limitOrder(1, Side::Sell, 100, 10));
    book.addOrder(limitOrder(2, Side::Sell, 100, 10));
    std::vector<Trade> trades;
    EXPECT_EQ(book.modifyOrder(1, 100, 10, trades).status, OrderStatus::Accepted);
    EXPECT_EQ(book.snapshot(Side::Sell)[0].id, 1u);
}

TEST(ModifyTest, ChangingPriceMovesOrderToNewLevel) {
    OrderBook book;
    book.addOrder(limitOrder(1, Side::Buy, 100, 10));
    book.addOrder(limitOrder(2, Side::Buy, 99, 10));

    std::vector<Trade> trades;
    auto m = book.modifyOrder(2, 101, 10, trades);
    EXPECT_EQ(m.status, OrderStatus::Accepted);
    EXPECT_EQ(book.getBestBid(), 101);
    EXPECT_EQ(book.getVolumeAt(Side::Buy, 99), 0);
    EXPECT_EQ(book.getLevelCount(Side::Buy), 2u);
}

TEST(ModifyTest, ModifyingToCrossingPriceMatchesImmediately) {
    OrderBook book;
    book.addOrder(limitOrder(1, Side::Sell, 100, 6));
    book.addOrder(limitOrder(2, Side::Buy, 95, 10));

    std::vector<Trade> trades;
    auto m = book.modifyOrder(2, 100, 10, trades);
    EXPECT_EQ(m.status, OrderStatus::Accepted);
    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].buyOrderId, 2u);
    EXPECT_EQ(trades[0].sellOrderId, 1u);
    EXPECT_EQ(trades[0].price, 100);
    EXPECT_EQ(trades[0].quantity, 6);
    EXPECT_EQ(m.restingQuantity, 4);
    EXPECT_EQ(book.getBestBid(), 100);
    EXPECT_FALSE(book.getBestAsk().has_value());
}

TEST(ModifyTest, UnknownOrderIsRejected) {
    OrderBook book;
    std::vector<Trade> trades;
    auto m = book.modifyOrder(42, 100, 10, trades);
    EXPECT_EQ(m.status, OrderStatus::RejectedUnknownOrder);
    EXPECT_EQ(m.sequence, 0u);
}

TEST(ModifyTest, FilledOrderCannotBeModified) {
    OrderBook book;
    book.addOrder(limitOrder(1, Side::Sell, 100, 5));
    book.addOrder(limitOrder(2, Side::Buy, 100, 5));
    std::vector<Trade> trades;
    EXPECT_EQ(book.modifyOrder(1, 100, 3, trades).status, OrderStatus::RejectedUnknownOrder);
}

TEST(ModifyTest, InvalidValuesAreRejectedAndLeaveOrderUntouched) {
    OrderBook book;
    book.addOrder(limitOrder(1, Side::Buy, 100, 10));
    std::vector<Trade> trades;

    EXPECT_EQ(book.modifyOrder(1, 100, 0, trades).status, OrderStatus::RejectedInvalidQuantity);
    EXPECT_EQ(book.modifyOrder(1, 100, -4, trades).status, OrderStatus::RejectedInvalidQuantity);
    EXPECT_EQ(book.modifyOrder(1, 0, 10, trades).status, OrderStatus::RejectedInvalidPrice);
    EXPECT_EQ(book.modifyOrder(1, -1, 10, trades).status, OrderStatus::RejectedInvalidPrice);

    auto snap = book.snapshot(Side::Buy);
    ASSERT_EQ(snap.size(), 1u);
    EXPECT_EQ(snap[0].price, 100);
    EXPECT_EQ(snap[0].quantity, 10);
}

TEST(ModifyTest, ModifyEventsConsumeSequenceNumbers) {
    OrderBook book;
    book.addOrder(limitOrder(1, Side::Buy, 100, 10));   // seq 1
    std::vector<Trade> trades;
    auto m = book.modifyOrder(1, 100, 5, trades);        // in-place reduce: seq 2
    EXPECT_EQ(m.sequence, 2u);
    EXPECT_EQ(book.lastSequence(), 2u);
    auto m2 = book.modifyOrder(1, 101, 5, trades);       // remove + resubmit: seq 3
    EXPECT_EQ(m2.sequence, 3u);
    EXPECT_EQ(book.snapshot(Side::Buy)[0].sequence, 3u);   // new time priority
}
