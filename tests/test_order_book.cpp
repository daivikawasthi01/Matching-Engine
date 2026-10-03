#include <iostream>
#include <cassert>
#include "order_book.h"

void test_basic_match() {
    OrderBook book;
    auto t1 = book.addOrder(Order{1, Side::Sell, 100.0, 10});
    assert(t1.empty());
    auto t2 = book.addOrder(Order{2, Side::Buy, 100.0, 10});
    assert(t2.size() == 1);
    assert(t2[0].price == 100.0);
    assert(t2[0].quantity == 10);
    assert(t2[0].buyOrderId == 2);
    assert(t2[0].sellOrderId == 1);
    std::cout << "[PASS] test_basic_match\n";
}

void test_partial_fill_incoming_larger() {
    OrderBook book;
    book.addOrder(Order{1, Side::Sell, 100.0, 5});
    auto t = book.addOrder(Order{2, Side::Buy, 100.0, 15});
    assert(t.size() == 1);
    assert(t[0].quantity == 5);
    // Remaining 10 rests on buy side; match against new sell
    auto t2 = book.addOrder(Order{3, Side::Sell, 100.0, 10});
    assert(t2.size() == 1);
    assert(t2[0].quantity == 10);
    assert(t2[0].buyOrderId == 2);
    assert(t2[0].sellOrderId == 3);
    std::cout << "[PASS] test_partial_fill_incoming_larger\n";
}

void test_partial_fill_resting_larger() {
    OrderBook book;
    book.addOrder(Order{1, Side::Sell, 100.0, 20});
    auto t = book.addOrder(Order{2, Side::Buy, 100.0, 8});
    assert(t.size() == 1);
    assert(t[0].quantity == 8);
    // Remaining 12 on sell side
    auto t2 = book.addOrder(Order{3, Side::Buy, 100.0, 12});
    assert(t2.size() == 1);
    assert(t2[0].quantity == 12);
    std::cout << "[PASS] test_partial_fill_resting_larger\n";
}

void test_multi_order_sweeping() {
    OrderBook book;
    book.addOrder(Order{1, Side::Sell, 99.0, 5});
    book.addOrder(Order{2, Side::Sell, 100.0, 10});
    book.addOrder(Order{3, Side::Sell, 101.0, 15});

    auto trades = book.addOrder(Order{4, Side::Buy, 101.0, 20});
    assert(trades.size() == 3); // sweeps 99.0 (5), 100.0 (10), and partial 101.0 (5) = 3 trade fills
    // Since sellOrders are stored sequentially, front order is matched first
    std::cout << "[PASS] test_multi_order_sweeping\n";
}

void test_cancel_order() {
    OrderBook book;
    book.addOrder(Order{1, Side::Sell, 100.0, 10});
    bool cancelled = book.cancelOrder(1);
    assert(cancelled == true);
    // Try matching after cancel
    auto t = book.addOrder(Order{2, Side::Buy, 100.0, 10});
    assert(t.empty());
    std::cout << "[PASS] test_cancel_order\n";
}

void test_cancel_non_existent() {
    OrderBook book;
    bool cancelled = book.cancelOrder(999);
    assert(cancelled == false);
    std::cout << "[PASS] test_cancel_non_existent\n";
}

void test_no_price_crossing() {
    OrderBook book;
    book.addOrder(Order{1, Side::Sell, 105.0, 10});
    auto t = book.addOrder(Order{2, Side::Buy, 100.0, 10});
    assert(t.empty());
    std::cout << "[PASS] test_no_price_crossing\n";
}

int main() {
    test_basic_match();
    test_partial_fill_incoming_larger();
    test_partial_fill_resting_larger();
    test_multi_order_sweeping();
    test_cancel_order();
    test_cancel_non_existent();
    test_no_price_crossing();
    std::cout << "All 7 Matching Engine unit tests passed successfully!\n";
    return 0;
}
