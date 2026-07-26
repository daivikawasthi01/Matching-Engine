#include "order_book.h"
#include <algorithm> //required for min & find_if


//OrderBook:: - means this method belongs to OrderBook class
std::vector<Trade> OrderBook::addOrder(const Order& order){
    std::vector<Trade> trades; //collects all trades executed during this single order insertion
    Order incoming = order; //Since const order, create a modifiable(mutable) copy
    
    if(incoming.side == Side::Buy){

        //Check if the buyer is willing to pay atleast as much as seller's asking price
        //Use a while loop to match as many resting orders as needed. Keep matching as long as incoming order has qty left to fill.
        while(incoming.quantity > 0 && !sellOrders.empty() && incoming.price >= sellOrders.front().price){
            
            // Grab a non-const reference (&) so we can modify the resting order's quantity directly inside the vector
            Order& resting = sellOrders.front();

            // Executed quantity is limited by whichever order has less quantity remaining
            int filledQty = std::min(incoming.quantity, resting.quantity);

            //Create and record trade receipt
            trades.push_back(Trade{incoming.id, resting.id, resting.price, filledQty});

            // Deduct the matched quantity from both orders
            incoming.quantity -= filledQty;
            resting.quantity -= filledQty;

            //if resting order fully filled, pop it from front of vector
            if(resting.quantity == 0){
                sellOrders.erase(sellOrders.begin());
            }
        }

        //if incoming buy order not fully filled, store whatever qty remains on the book
        if(incoming.quantity > 0){
            buyOrders.push_back(incoming);
        }  
    } else{
        //Use a while loop to match as many incoming orders(as long as qty remaining)
        //Check if seller's asking price is less than or equal to what buyer is offering
        while(incoming.quantity > 0 && !buyOrders.empty() && incoming.price <= buyOrders.front().price){
            
            //Do the same thing for match as sell
            Order& resting = buyOrders.front();
            int filledQty = std::min(incoming.quantity, resting.quantity);

            //for sell orders, resting order is the buyer, incoming order is the seller
            trades.push_back(Trade{resting.id, incoming.id, resting.price, filledQty});

            incoming.quantity -= filledQty;
            resting.quantity -= filledQty;
            
            if(resting.quantity == 0){
                buyOrders.erase(buyOrders.begin());
            }
        }
        //if incoming sell order not fully fulfilled, rest remaining quantity on the book
        if(incoming.quantity > 0){
            sellOrders.push_back(incoming);
        }
    }

    // return all execution receipts (will be empty if no match occured)
    return trades;
}

bool OrderBook::cancelOrder(int orderId){
    // Lambda function (inline anonymous function) that searches a vector for an order by ID and erases it
    // [orderId] captures the target ID from the outer function
    // (std::vector<Order>& orders) accepts either buyOrders or sellOrders by reference
    auto removeById = [orderId](std::vector<Order>& orders){

        // std::find_if iterates from begin() to end(), stopping when the lambda predicate returns true
        auto it = std::find_if(orders.begin(), orders.end(), [orderId](const Order& o){
            return o.id == orderId;
        });

        // If std::find_if didn't reach end(), it found a match
        if (it != orders.end()) {
            orders.erase(it); // Remove the order from the vector
            return true;      // Cancel successful
        }
        return false;         // Order ID wasn't in this vector
    };

    // Try finding and removing from buyOrders first; if not found, try sellOrders
    if(removeById(buyOrders)) return true;
    return removeById(sellOrders);
}