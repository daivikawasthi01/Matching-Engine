#include "order_book.h"
#include <optional>
//optional - this might have value or might have nothing
//because addOrder might produce a trade or might not

//OrderBook:: - means this method belongs to OrderBook class
std::optional<Trade> OrderBook::addOrder(const Order& order){
    if(order.side == Side::Buy){

        //Check if the buyer is willing to pay atleast as much as seller's asking price
        if(!sellOrders.empty() && order.price >= sellOrders.front().price){
            //execute the match - copy counter order from front of sellorders
            Order matched = sellOrders.front();
            //removes that matched sell order from book so that cant match twice
            sellOrders.erase(sellOrders.begin());
            //return new trade receipt
            return Trade{order.id, matched.id, matched.price, matched.quantity};
        }
        //if no sellers or no match, save order into buyorder vector for future matches
        buyOrders.push_back(order);
    } else{

        //Check if seller's asking price is less than or equal to what buyer is offering
        if(!buyOrders.empty() && order.price <= buyOrders.front().price){
            //Do the same thing for match as sell
            Order matched = buyOrders.front();
            buyOrders.erase(buyOrders.begin());
            //return trade receipt
            return Trade{matched.id, order.id, matched.price, matched.quantity};
        }
        //if no match or no buyers, save into sellorder vector for future
        sellOrders.push_back(order);
    }
    //if no match for sell or buy then return nullopt - an optional object doesnt contain any value
    return std::nullopt;
}