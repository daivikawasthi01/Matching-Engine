#include <iostream>
#include <vector>
#include <iomanip>
#include "../include/order_book.h"

void printTrades(const std::vector<Trade>& trades) {
    if (trades.empty()) {
        std::cout << "  -> No match (order rested or expired).\n";
        return;
    }
    for (const auto& t : trades) {
        std::cout << "  -> Trade Executed: Buyer #" << t.buyOrderId 
                  << " <-> Seller #" << t.sellOrderId 
                  << " | Price: $" << std::fixed << std::setprecision(2) << t.price 
                  << " | Qty: " << t.quantity << "\n";
    }
}

void printBookStatus(const OrderBook& book) {
    std::cout << "  [Book Status] Active Orders: " << book.getOrderCount();
    if (book.getBestBid().has_value()) {
        std::cout << " | Best Bid: $" << *book.getBestBid();
    } else {
        std::cout << " | Best Bid: None";
    }
    if (book.getBestAsk().has_value()) {
        std::cout << " | Best Ask: $" << *book.getBestAsk();
    } else {
        std::cout << " | Best Ask: None";
    }
    std::cout << "\n";
}

int main() {
    OrderBook book;

    std::cout << "=======================================================\n";
    std::cout << "       Deterministic C++17 Matching Engine Demo        \n";
    std::cout << "=======================================================\n\n";

    // Scenario 1: Placing resting limit sell orders at multiple price levels
    std::cout << "1. Placing Resting Limit Sell Orders (Asks):\n";
    std::cout << "   - Order 1: Sell 10 @ $102.00\n";
    printTrades(book.addOrder(Order{1, Side::Sell, 102.0, 10}));
    std::cout << "   - Order 2: Sell 10 @ $100.00\n";
    printTrades(book.addOrder(Order{2, Side::Sell, 100.0, 10}));
    std::cout << "   - Order 3: Sell 15 @ $101.00\n";
    printTrades(book.addOrder(Order{3, Side::Sell, 101.0, 15}));
    printBookStatus(book);

    // Scenario 2: Price-Time priority sweep
    std::cout << "\n2. Submitting Large Buy Limit Order (Sweeping Best Prices):\n";
    std::cout << "   - Order 4: Buy 20 @ $101.50\n";
    printTrades(book.addOrder(Order{4, Side::Buy, 101.5, 20}));
    printBookStatus(book);

    // Scenario 3: Market Order Execution
    std::cout << "\n3. Submitting Market Buy Order (Immediate Liquidity Taker):\n";
    std::cout << "   - Order 5: Market Buy 10 units\n";
    printTrades(book.addOrder(Order{5, Side::Buy, 0.0, 10, OrderType::Market}));
    printBookStatus(book);

    // Scenario 4: Fast O(log P) Cancellation
    std::cout << "\n4. Fast Order Cancellation:\n";
    std::cout << "   - Order 6: Buy 50 @ $98.00 (Resting Bid)\n";
    printTrades(book.addOrder(Order{6, Side::Buy, 98.0, 50}));
    printBookStatus(book);

    std::cout << "   - Cancelling Order 6 by ID:\n";
    bool cancelled = book.cancelOrder(6);
    std::cout << "     Cancel status: " << (cancelled ? "SUCCESS (Removed in O(log P))" : "FAILED") << "\n";
    printBookStatus(book);

    std::cout << "\n=======================================================\n";
    std::cout << "Demo completed successfully.\n";
    return 0;
}