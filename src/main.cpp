#include <iostream>
#include <vector>
#include "order_book.h"

void printTrades(const std::vector<Trade>& trades) {
    if (trades.empty()) {
        std::cout << "No trade.\n";
    }
    for (const auto& t : trades) {
        std::cout << "Trade executed: buy#" << t.buyOrderId 
                  << " sell#" << t.sellOrderId 
                  << " price=" << t.price 
                  << " qty=" << t.quantity << "\n";
    }
}

int main() {
    // Instantiate an OrderBook object
    OrderBook book;

    std::cout << "--- Scenario: Limit Order Matching and Cancellation ---\n";

    // 1. Order 1 (Sell): 10 units at $100.00. No buyers available, rests on sell side.
    std::cout << "\nSubmitting Order 1 (Sell 10 @ 100.0):\n";
    printTrades(book.addOrder(Order{1, Side::Sell, 100.0, 10}));

    // 2. Order 2 (Buy): 15 units at $100.00. Crosses with Order 1.
    // Trades 10 units against Order 1. Order 1 is filled; remaining 5 units of Order 2 rest.
    std::cout << "\nSubmitting Order 2 (Buy 15 @ 100.0):\n";
    printTrades(book.addOrder(Order{2, Side::Buy, 100.0, 15}));

    // 3. Cancel Order 2: The remaining 5 resting units are removed from the book.
    std::cout << "\nCancelling Order 2:\n";
    bool cancelled = book.cancelOrder(2);
    std::cout << "Cancel order 2: " << (cancelled ? "success" : "not found") << "\n";

    return 0;
}