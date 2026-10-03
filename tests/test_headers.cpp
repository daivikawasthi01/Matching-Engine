// Header compilation test: verifies that each header is self-contained and compiles under C++17
#include "order.h"
#include "trade.h"
#include "order_book.h"
#include <iostream>
#include <cassert>

void test_header_types() {
    Order o{1, Side::Buy, 100.5, 10, OrderType::Limit};
    assert(o.id == 1);
    assert(o.side == Side::Buy);
    assert(o.price == 100.5);
    assert(o.quantity == 10);
    assert(o.type == OrderType::Limit);

    Trade t{1, 2, 100.5, 10};
    assert(t.buyOrderId == 1);
    assert(t.sellOrderId == 2);
    assert(t.price == 100.5);
    assert(t.quantity == 10);

    OrderBook book;
    assert(book.empty());
    assert(book.getOrderCount() == 0);
    assert(!book.getBestBid().has_value());
    assert(!book.getBestAsk().has_value());

    std::cout << "[PASS] test_header_types & self-containment\n";
}

int main() {
    std::cout << "===== Running Header Self-Containment Test =====\n";
    test_header_types();
    std::cout << "===== Header Test Passed =====\n";
    return 0;
}
