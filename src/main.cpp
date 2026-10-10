#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

#include "order_book.h"

// Prices are integer ticks; this demo uses a tick size of $0.01.
static std::string dollars(Price ticks) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "$%lld.%02lld", static_cast<long long>(ticks / 100),
                  static_cast<long long>(ticks % 100));
    return buf;
}

static void printResult(const AddResult& r) {
    std::cout << "  -> " << toString(r.status) << " (seq " << r.sequence << ")";
    if (r.restingQuantity > 0) std::cout << ", " << r.restingQuantity << " resting";
    std::cout << "\n";
    for (const auto& t : r.trades) {
        std::cout << "     Trade #" << t.sequence << ": buyer " << t.buyOrderId << " <-> seller "
                  << t.sellOrderId << " | " << dollars(t.price) << " | qty " << t.quantity << "\n";
    }
}

static void printBook(const OrderBook& book) {
    std::cout << "  [Book] orders: " << book.getOrderCount();
    std::cout << " | best bid: "
              << (book.getBestBid() ? dollars(*book.getBestBid()) : std::string("none"));
    std::cout << " | best ask: "
              << (book.getBestAsk() ? dollars(*book.getBestAsk()) : std::string("none")) << "\n";
}

int main() {
    OrderBook book;

    std::cout << "=======================================================\n";
    std::cout << "        Deterministic C++17 Matching Engine Demo       \n";
    std::cout << "=======================================================\n\n";

    std::cout << "1. Resting limit sell orders at three price levels:\n";
    std::cout << "   Sell 10 @ $102.00, Sell 10 @ $100.00, Sell 15 @ $101.00\n";
    printResult(book.addOrder(Order{1, Side::Sell, 10200, 10}));
    printResult(book.addOrder(Order{2, Side::Sell, 10000, 10}));
    printResult(book.addOrder(Order{3, Side::Sell, 10100, 15}));
    printBook(book);

    std::cout << "\n2. Buy 20 @ $101.50 sweeps the best prices first:\n";
    printResult(book.addOrder(Order{4, Side::Buy, 10150, 20}));
    printBook(book);

    std::cout << "\n3. Market buy 10 takes whatever liquidity is best:\n";
    printResult(book.addOrder(Order{5, Side::Buy, 0, 10, OrderType::Market}));
    printBook(book);

    std::cout << "\n4. Immediate-or-cancel: Buy 50 @ $103.00 IOC fills what it can, drops the rest:\n";
    printResult(book.addOrder(Order{6, Side::Buy, 10300, 50, OrderType::Limit,
                                    TimeInForce::ImmediateOrCancel}));
    printBook(book);

    std::cout << "\n5. Fill-or-kill: Buy 5 @ $99.00 FOK cannot fill, so nothing happens:\n";
    book.addOrder(Order{7, Side::Sell, 10100, 8});
    printResult(book.addOrder(Order{8, Side::Buy, 9900, 5, OrderType::Limit,
                                    TimeInForce::FillOrKill}));
    printBook(book);

    std::cout << "\n6. Modify and cancel:\n";
    std::cout << "   Rest Buy 50 @ $98.00 (order 9), then reduce it to 30 in place\n";
    printResult(book.addOrder(Order{9, Side::Buy, 9800, 50}));
    std::vector<Trade> trades;
    const OrderResult m = book.modifyOrder(9, 9800, 30, trades);
    std::cout << "  -> modify: " << toString(m.status) << " (seq " << m.sequence << "), "
              << m.restingQuantity << " resting, time priority kept\n";
    std::cout << "   Cancel order 9: " << (book.cancelOrder(9) ? "removed" : "not found") << "\n";
    printBook(book);

    std::cout << "\n=======================================================\n";
    std::cout << "Demo completed. Last engine sequence number: " << book.lastSequence() << "\n";
    return 0;
}
