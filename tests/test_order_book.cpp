#include <iostream>
#include <cassert>
#include "../include/order_book.h"

void test_basic_match() {
    OrderBook book;
    auto t1 = book.addOrder(Order{1, Side::Sell, 100.0, 10});
    assert(t1.empty());
    assert(book.getOrderCount() == 1);
    assert(book.getBestAsk() == 100.0);

    auto t2 = book.addOrder(Order{2, Side::Buy, 100.0, 10});
    assert(t2.size() == 1);
    assert(t2[0].price == 100.0);
    assert(t2[0].quantity == 10);
    assert(t2[0].buyOrderId == 2);
    assert(t2[0].sellOrderId == 1);
    assert(book.empty());
    std::cout << "[PASS] test_basic_match\n";
}

void test_partial_fill_and_resting() {
    OrderBook book;
    book.addOrder(Order{1, Side::Sell, 100.0, 10});
    auto t2 = book.addOrder(Order{2, Side::Buy, 100.0, 15});
    assert(t2.size() == 1);
    assert(t2[0].quantity == 10);
    assert(book.getOrderCount() == 1);
    assert(book.getBestBid() == 100.0);
    std::cout << "[PASS] test_partial_fill_and_resting\n";
}

void test_price_priority() {
    OrderBook book;
    // Insert sell orders in non-sorted price order
    book.addOrder(Order{1, Side::Sell, 105.0, 10});
    book.addOrder(Order{2, Side::Sell, 101.0, 10});
    book.addOrder(Order{3, Side::Sell, 103.0, 10});

    // Best ask should be 101.0
    assert(book.getBestAsk() == 101.0);

    // Buy order at 104.0 for 15 shares should match 101.0 (10 shares) and 103.0 (5 shares)
    auto trades = book.addOrder(Order{4, Side::Buy, 104.0, 15});
    assert(trades.size() == 2);
    assert(trades[0].sellOrderId == 2 && trades[0].price == 101.0 && trades[0].quantity == 10);
    assert(trades[1].sellOrderId == 3 && trades[1].price == 103.0 && trades[1].quantity == 5);

    // Remaining on book: Order 3 (5 @ 103.0) and Order 1 (10 @ 105.0)
    assert(book.getBestAsk() == 103.0);
    std::cout << "[PASS] test_price_priority\n";
}

void test_time_priority() {
    OrderBook book;
    // Two buy orders at the same price: Order 1 arrives before Order 2
    book.addOrder(Order{1, Side::Buy, 100.0, 10});
    book.addOrder(Order{2, Side::Buy, 100.0, 10});

    // Incoming Sell for 15 shares should fully fill Order 1 (10) and then partially fill Order 2 (5)
    auto trades = book.addOrder(Order{3, Side::Sell, 100.0, 15});
    assert(trades.size() == 2);
    assert(trades[0].buyOrderId == 1 && trades[0].quantity == 10);
    assert(trades[1].buyOrderId == 2 && trades[1].quantity == 5);

    // Order 2 still has 5 resting shares
    assert(book.getOrderCount() == 1);
    assert(book.getBestBid() == 100.0);
    std::cout << "[PASS] test_time_priority\n";
}

void test_multi_level_sweep() {
    OrderBook book;
    book.addOrder(Order{1, Side::Sell, 100.0, 10});
    book.addOrder(Order{2, Side::Sell, 101.0, 20});
    book.addOrder(Order{3, Side::Sell, 102.0, 30});

    // Buy 45 shares at limit 103.0 -> sweeps 10 @ 100.0, 20 @ 101.0, 15 @ 102.0
    auto trades = book.addOrder(Order{4, Side::Buy, 103.0, 45});
    assert(trades.size() == 3);
    assert(trades[0].price == 100.0 && trades[0].quantity == 10);
    assert(trades[1].price == 101.0 && trades[1].quantity == 20);
    assert(trades[2].price == 102.0 && trades[2].quantity == 15);

    // Order 3 has 15 shares left @ 102.0
    assert(book.getOrderCount() == 1);
    assert(book.getBestAsk() == 102.0);
    std::cout << "[PASS] test_multi_level_sweep\n";
}

void test_market_orders() {
    OrderBook book;
    book.addOrder(Order{1, Side::Sell, 100.0, 10});
    book.addOrder(Order{2, Side::Sell, 102.0, 10});

    // Market Buy order for 15 units (price can be 0.0)
    auto trades = book.addOrder(Order{3, Side::Buy, 0.0, 15, OrderType::Market});
    assert(trades.size() == 2);
    assert(trades[0].price == 100.0 && trades[0].quantity == 10);
    assert(trades[1].price == 102.0 && trades[1].quantity == 5);

    // Remaining on book: 5 @ 102.0 (Order 2)
    assert(book.getOrderCount() == 1);
    assert(book.getBestAsk() == 102.0);

    // Market Buy order for 20 units when only 5 units exist -> fills 5, remainder does NOT rest
    auto trades2 = book.addOrder(Order{4, Side::Buy, 0.0, 20, OrderType::Market});
    assert(trades2.size() == 1);
    assert(trades2[0].quantity == 5);
    assert(book.empty()); // Book is completely empty, remainder was discarded
    std::cout << "[PASS] test_market_orders\n";
}

void test_cancellation() {
    OrderBook book;
    book.addOrder(Order{1, Side::Buy, 99.0, 10});
    book.addOrder(Order{2, Side::Buy, 100.0, 15});
    book.addOrder(Order{3, Side::Sell, 105.0, 20});

    assert(book.getOrderCount() == 3);
    assert(book.getBestBid() == 100.0);

    // Cancel best bid (Order 2)
    bool cancelled = book.cancelOrder(2);
    assert(cancelled);
    assert(book.getOrderCount() == 2);
    assert(book.getBestBid() == 99.0);

    // Cancel non-existent order
    bool notFound = book.cancelOrder(999);
    assert(!notFound);

    // Cancel already cancelled order
    bool alreadyCancelled = book.cancelOrder(2);
    assert(!alreadyCancelled);

    // Cancel remaining orders
    assert(book.cancelOrder(1));
    assert(book.cancelOrder(3));
    assert(book.empty());
    assert(!book.getBestBid().has_value());
    assert(!book.getBestAsk().has_value());
    std::cout << "[PASS] test_cancellation\n";
}

void test_reject_invalid_quantity() {
    OrderBook book;
    // Test quantity = 0
    auto t1 = book.addOrder(Order{1, Side::Buy, 100.0, 0});
    assert(t1.empty());
    assert(book.empty());

    // Test negative quantity
    auto t2 = book.addOrder(Order{2, Side::Sell, 100.0, -10});
    assert(t2.empty());
    assert(book.empty());

    // Test market order with 0 / negative quantity
    auto t3 = book.addOrder(Order{3, Side::Buy, 0.0, 0, OrderType::Market});
    assert(t3.empty());
    assert(book.empty());

    std::cout << "[PASS] test_reject_invalid_quantity\n";
}

void test_reject_duplicate_order_id() {
    OrderBook book;
    // Add first order with ID 1
    auto t1 = book.addOrder(Order{1, Side::Buy, 100.0, 10});
    assert(t1.empty());
    assert(book.getOrderCount() == 1);

    // Attempt to add duplicate order with ID 1
    auto t2 = book.addOrder(Order{1, Side::Buy, 105.0, 20});
    assert(t2.empty());
    assert(book.getOrderCount() == 1);
    assert(book.getBestBid() == 100.0); // original order remains untouched

    // Attempt to add duplicate order with ID 1 as sell
    auto t3 = book.addOrder(Order{1, Side::Sell, 100.0, 10});
    assert(t3.empty());
    assert(book.getOrderCount() == 1); // no match occurred

    // Add order with different ID 2 -> should succeed
    auto t4 = book.addOrder(Order{2, Side::Sell, 100.0, 10});
    assert(t4.size() == 1);
    assert(book.empty());

    // After ID 1 is fully filled and removed from book, ID 1 can be used again (or if cancelled)
    auto t5 = book.addOrder(Order{1, Side::Sell, 102.0, 5});
    assert(t5.empty());
    assert(book.getOrderCount() == 1);

    std::cout << "[PASS] test_reject_duplicate_order_id\n";
}

int main() {
    std::cout << "===== Running Matching Engine Unit Tests =====\n";
    test_basic_match();
    test_partial_fill_and_resting();
    test_price_priority();
    test_time_priority();
    test_multi_level_sweep();
    test_market_orders();
    test_cancellation();
    test_reject_invalid_quantity();
    test_reject_duplicate_order_id();
    std::cout << "===== ALL TESTS PASSED =====\n";
    return 0;
}
