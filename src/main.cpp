#include <iostream>
#include "order_book.h"

void printTrades(const std::vector<Trade>& trades){
    if(trades.empty()){
        std::cout << "No trade.\n";
    }
    for(const auto& t : trades){
        std::cout << "Trade: buy#" << t.buyOrderId << " sell#" << t.sellOrderId << " price=" << t.price << " qty=" << t.quantity << "\n";
    }
}
int main() {
    //instantiate an OrderBook object named book. this initializes two empty vectors for buy and sell orders 
    OrderBook book;

    printTrades(book.addOrder(Order{1, Side::Sell, 100.0, 10}));
    printTrades(book.addOrder(Order{2, Side::Buy, 100.0, 15}));

    bool cancelled = book.cancelOrder(2);
    std::cout << "Cancel order 2: " << (cancelled ? "success" : "not found") << "\n";

    return 0;
}

    //sellorder object
    
    //check if any resting buy orders >= 100, since buyorders empty no match. sellOrder saved into sellorders vector and addOrder returns nullopt
    // auto infers the type as std::optional<Trade>.
    // In a boolean context (result1 ? "yes" : "no"), std::optional evaluates to true if it holds a value, or false if it is std::nullopt.
    
    // Prints: Sell order added. Trade? no
    
    //buyorder Object
    //book checks sellOrders. It finds order #1 resting at price 100.0
    // since buyer price >= sellers price, matched(both are 100)
    //sellorder removed from sellorders and a trade object returned

    //since std::optional<Trade>, you use arrow vector not . to access internal fields of Trade struct
    //prints Trade executed: buy#2 sell#1 price=100 qty=10
    return 0;
}