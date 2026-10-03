#include <iostream>
#include <cassert>
#include <vector>
#include "../include/order_book.h"

void test_market_sell_sweep() {
    OrderBook book;
    book.addOrder(Order{1, Side::Buy, 100.0, 10});
    book.addOrder(Order{2, Side::Buy, 99.0, 20});
    book.addOrder(Order{3, Side::Buy, 98.0, 30});

    // Market Sell order sweeps top bids
    auto trades = book.addOrder(Order{4, Side::Sell, 0.0, 25, OrderType::Market});
    assert(trades.size() == 2);
    // Sweeps 10 @ 100.0 first
    assert(trades[0].buyOrderId == 1 && trades[0].price == 100.0 && trades[0].quantity == 10);
    // Sweeps 15 @ 99.0 next
    assert(trades[1].buyOrderId == 2 && trades[1].price == 99.0 && trades[1].quantity == 15);

    // Remaining in book: 5 @ 99.0 (Order 2) and 30 @ 98.0 (Order 3)
    assert(book.getOrderCount() == 2);
    assert(book.getBestBid().has_value() && *book.getBestBid() == 99.0);
    std::cout << "[PASS] test_market_sell_sweep\n";
}

void test_market_order_on_empty_book() {
    OrderBook book;
    // Market Buy on empty book -> 0 trades, does not rest
    auto tradesBuy = book.addOrder(Order{1, Side::Buy, 0.0, 10, OrderType::Market});
    assert(tradesBuy.empty());
    assert(book.empty());

    // Market Sell on empty book -> 0 trades, does not rest
    auto tradesSell = book.addOrder(Order{2, Side::Sell, 0.0, 10, OrderType::Market});
    assert(tradesSell.empty());
    assert(book.empty());
    std::cout << "[PASS] test_market_order_on_empty_book\n";
}

void test_cancel_queue_positions() {
    OrderBook book;
    // Place 3 buy orders at the exact same price $100.0
    book.addOrder(Order{1, Side::Buy, 100.0, 10});
    book.addOrder(Order{2, Side::Buy, 100.0, 20});
    book.addOrder(Order{3, Side::Buy, 100.0, 30});

    assert(book.getOrderCount() == 3);

    // 1. Cancel middle order (Order 2)
    assert(book.cancelOrder(2));
    assert(book.getOrderCount() == 2);

    // Incoming Sell of 15 should match Order 1 (10) and Order 3 (5), skipping Order 2
    auto trades = book.addOrder(Order{4, Side::Sell, 100.0, 15});
    assert(trades.size() == 2);
    assert(trades[0].buyOrderId == 1 && trades[0].quantity == 10);
    assert(trades[1].buyOrderId == 3 && trades[1].quantity == 5);

    // Order 3 should have 25 units left
    assert(book.getOrderCount() == 1);
    assert(*book.getBestBid() == 100.0);

    // 2. Cancel remaining order (Order 3)
    assert(book.cancelOrder(3));
    assert(book.empty());
    assert(!book.getBestBid().has_value());
    std::cout << "[PASS] test_cancel_queue_positions (head, middle, tail)\n";
}

void test_partial_fill_then_cancel() {
    OrderBook book;
    book.addOrder(Order{1, Side::Sell, 105.0, 50});

    // Buy 20 -> leaves 30 on Order 1
    auto trades = book.addOrder(Order{2, Side::Buy, 105.0, 20});
    assert(trades.size() == 1);
    assert(trades[0].quantity == 20);
    assert(book.getOrderCount() == 1);

    // Cancel remaining 30 units of Order 1
    assert(book.cancelOrder(1));
    assert(book.empty());
    assert(!book.getBestAsk().has_value());
    std::cout << "[PASS] test_partial_fill_then_cancel\n";
}

void test_multiple_price_levels_bid_and_ask() {
    OrderBook book;
    // Add multiple bids
    book.addOrder(Order{1, Side::Buy, 95.0, 10});
    book.addOrder(Order{2, Side::Buy, 97.0, 15});
    book.addOrder(Order{3, Side::Buy, 96.0, 20});

    // Add multiple asks
    book.addOrder(Order{4, Side::Sell, 102.0, 10});
    book.addOrder(Order{5, Side::Sell, 100.0, 15});
    book.addOrder(Order{6, Side::Sell, 101.0, 20});

    assert(book.getOrderCount() == 6);
    assert(*book.getBestBid() == 97.0);
    assert(*book.getBestAsk() == 100.0);

    // Non-crossing limit order
    auto noTrades = book.addOrder(Order{7, Side::Buy, 99.0, 5});
    assert(noTrades.empty());
    assert(*book.getBestBid() == 99.0);

    // Crossing order that sweeps multiple asks
    auto sweepTrades = book.addOrder(Order{8, Side::Buy, 101.5, 30});
    assert(sweepTrades.size() == 2);
    assert(sweepTrades[0].sellOrderId == 5 && sweepTrades[0].price == 100.0 && sweepTrades[0].quantity == 15);
    assert(sweepTrades[1].sellOrderId == 6 && sweepTrades[1].price == 101.0 && sweepTrades[1].quantity == 15);

    // Order 6 has 5 left @ 101.0
    assert(*book.getBestAsk() == 101.0);
    std::cout << "[PASS] test_multiple_price_levels_bid_and_ask\n";
}

int main() {
    std::cout << "===== Running Matching Engine Edge Case Tests =====\n";
    test_market_sell_sweep();
    test_market_order_on_empty_book();
    test_cancel_queue_positions();
    test_partial_fill_then_cancel();
    test_multiple_price_levels_bid_and_ask();
    std::cout << "===== All Edge Case Tests Passed =====\n";
    return 0;
}
