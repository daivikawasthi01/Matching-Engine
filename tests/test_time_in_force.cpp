// ImmediateOrCancel and FillOrKill behaviour.
#include "test_support.h"

namespace {
constexpr auto kIoc = TimeInForce::ImmediateOrCancel;
constexpr auto kFok = TimeInForce::FillOrKill;
}  // namespace

// --- ImmediateOrCancel -----------------------------------------------------

TEST(ImmediateOrCancelTest, PartialFillDiscardsRemainder) {
    OrderBook book;
    book.addOrder(limitOrder(1, Side::Sell, 100, 10));

    auto r = book.addOrder(limitOrder(2, Side::Buy, 100, 25, kIoc));
    EXPECT_EQ(r.status, OrderStatus::Accepted);
    ASSERT_EQ(r.trades.size(), 1u);
    EXPECT_EQ(r.trades[0].quantity, 10);
    EXPECT_EQ(r.restingQuantity, 0);
    EXPECT_TRUE(book.empty());           // remainder 15 was not rested
    EXPECT_FALSE(book.contains(2));
}

TEST(ImmediateOrCancelTest, NoCrossMeansNothingHappensAndNothingRests) {
    OrderBook book;
    book.addOrder(limitOrder(1, Side::Sell, 105, 10));

    auto r = book.addOrder(limitOrder(2, Side::Buy, 100, 10, kIoc));
    EXPECT_EQ(r.status, OrderStatus::Accepted);
    EXPECT_TRUE(r.trades.empty());
    EXPECT_EQ(r.restingQuantity, 0);
    EXPECT_EQ(book.getOrderCount(), 1u);
    EXPECT_FALSE(book.getBestBid().has_value());
}

TEST(ImmediateOrCancelTest, RespectsLimitPriceAcrossLevels) {
    OrderBook book;
    book.addOrder(limitOrder(1, Side::Sell, 100, 5));
    book.addOrder(limitOrder(2, Side::Sell, 101, 5));
    book.addOrder(limitOrder(3, Side::Sell, 102, 5));

    auto r = book.addOrder(limitOrder(4, Side::Buy, 101, 100, kIoc));
    EXPECT_EQ(totalQuantity(r.trades), 10);   // 102 is beyond the limit
    EXPECT_EQ(book.getBestAsk(), 102);
    EXPECT_EQ(book.getOrderCount(), 1u);
}

TEST(ImmediateOrCancelTest, MarketOrderWithGoodTillCancelNeverRests) {
    OrderBook book;
    book.addOrder(limitOrder(1, Side::Buy, 100, 5));
    auto r = book.addOrder(marketOrder(2, Side::Sell, 20, TimeInForce::GoodTillCancel));
    EXPECT_EQ(totalQuantity(r.trades), 5);
    EXPECT_EQ(r.restingQuantity, 0);
    EXPECT_TRUE(book.empty());
}

// --- FillOrKill ------------------------------------------------------------

TEST(FillOrKillTest, FullyFillableAcrossLevelsExecutes) {
    OrderBook book;
    book.addOrder(limitOrder(1, Side::Sell, 100, 5));
    book.addOrder(limitOrder(2, Side::Sell, 101, 5));
    book.addOrder(limitOrder(3, Side::Sell, 102, 5));

    auto r = book.addOrder(limitOrder(4, Side::Buy, 102, 12, kFok));
    EXPECT_EQ(r.status, OrderStatus::Accepted);
    EXPECT_EQ(totalQuantity(r.trades), 12);
    EXPECT_EQ(r.trades.size(), 3u);
    EXPECT_EQ(r.restingQuantity, 0);
    EXPECT_EQ(book.getVolumeAt(Side::Sell, 102), 3);
}

TEST(FillOrKillTest, ExactlyAvailableQuantityExecutes) {
    OrderBook book;
    book.addOrder(limitOrder(1, Side::Sell, 100, 5));
    book.addOrder(limitOrder(2, Side::Sell, 101, 5));
    auto r = book.addOrder(limitOrder(3, Side::Buy, 101, 10, kFok));
    EXPECT_EQ(r.status, OrderStatus::Accepted);
    EXPECT_EQ(totalQuantity(r.trades), 10);
    EXPECT_TRUE(book.empty());
}

TEST(FillOrKillTest, InsufficientLiquidityIsKilledWithNoSideEffects) {
    OrderBook book;
    book.addOrder(limitOrder(1, Side::Sell, 100, 5));
    book.addOrder(limitOrder(2, Side::Sell, 101, 5));
    const auto before = book.snapshot(Side::Sell);

    auto r = book.addOrder(limitOrder(3, Side::Buy, 101, 11, kFok));
    EXPECT_EQ(r.status, OrderStatus::Killed);
    EXPECT_TRUE(r.trades.empty());
    EXPECT_EQ(r.restingQuantity, 0);
    EXPECT_GT(r.sequence, 0u);                 // a killed order is still an engine event
    EXPECT_EQ(book.snapshot(Side::Sell), before);
    EXPECT_FALSE(book.contains(3));
    EXPECT_EQ(book.getVolumeAt(Side::Sell, 100), 5);
}

TEST(FillOrKillTest, LiquidityBeyondLimitPriceDoesNotCount) {
    OrderBook book;
    book.addOrder(limitOrder(1, Side::Sell, 100, 5));
    book.addOrder(limitOrder(2, Side::Sell, 110, 50));   // plenty, but beyond the limit

    auto r = book.addOrder(limitOrder(3, Side::Buy, 105, 10, kFok));
    EXPECT_EQ(r.status, OrderStatus::Killed);
    EXPECT_EQ(book.getVolumeAt(Side::Sell, 100), 5);
}

TEST(FillOrKillTest, WorksOnTheSellSide) {
    OrderBook book;
    book.addOrder(limitOrder(1, Side::Buy, 100, 5));
    book.addOrder(limitOrder(2, Side::Buy, 99, 5));

    EXPECT_EQ(book.addOrder(limitOrder(3, Side::Sell, 99, 11, kFok)).status, OrderStatus::Killed);
    auto ok = book.addOrder(limitOrder(4, Side::Sell, 99, 10, kFok));
    EXPECT_EQ(ok.status, OrderStatus::Accepted);
    EXPECT_EQ(totalQuantity(ok.trades), 10);
    EXPECT_TRUE(book.empty());
}

TEST(FillOrKillTest, MarketFillOrKillNeedsFullBookDepth) {
    OrderBook book;
    book.addOrder(limitOrder(1, Side::Sell, 100, 5));
    book.addOrder(limitOrder(2, Side::Sell, 500, 5));

    EXPECT_EQ(book.addOrder(marketOrder(3, Side::Buy, 11, kFok)).status, OrderStatus::Killed);
    auto ok = book.addOrder(marketOrder(4, Side::Buy, 10, kFok));
    EXPECT_EQ(ok.status, OrderStatus::Accepted);
    EXPECT_EQ(totalQuantity(ok.trades), 10);
}

TEST(FillOrKillTest, NeverRestsEvenWhenKilledOrFilled) {
    OrderBook book;
    EXPECT_EQ(book.addOrder(limitOrder(1, Side::Buy, 100, 10, kFok)).status, OrderStatus::Killed);
    EXPECT_TRUE(book.empty());
}
