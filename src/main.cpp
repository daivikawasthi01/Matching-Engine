#include <iostream>
#include "order_book.h"

int main() {
    //instantiate an OrderBook object named book. this initializes two empty vectors for buy and sell orders 
    OrderBook book;

    //sellorder object
    Order sellOrder{1, Side::Sell, 100.0, 10};
    //check if any resting buy orders >= 100, since buyorders empty no match. sellOrder saved into sellorders vector and addOrder returns nullopt
    // auto infers the type as std::optional<Trade>.
    // In a boolean context (result1 ? "yes" : "no"), std::optional evaluates to true if it holds a value, or false if it is std::nullopt.
    auto result1 = book.addOrder(sellOrder);
    // Prints: Sell order added. Trade? no
    std::cout << "Sell order added. Trade?" << (result1 ? "yes" : "no") << "\n";
    
    //buyorder Object
    Order buyOrder{2, Side::Buy, 100.0, 10};
    //book checks sellOrders. It finds order #1 resting at price 100.0
    // since buyer price >= sellers price, matched(both are 100)
    //sellorder removed from sellorders and a trade object returned
    auto result2 = book.addOrder(buyOrder);

    //since std::optional<Trade>, you use arrow vector not . to access internal fields of Trade struct
    //prints Trade executed: buy#2 sell#1 price=100 qty=10
    if(result2){
        std::cout << "Trade executed: buy#" << result2->buyOrderId 
        << " sell#" << result2->sellOrderId 
        << " price=" << result2->price
        << " qty=" << result2->quantity << "\n";
    } else {
        std::cout << "No trade.\n";
    }
    return 0;
}