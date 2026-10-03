#pragma once


// Side of the order in the book
enum class Side { 
    Buy, 
    Sell 
};

// Type of the order: Limit (rests on book at price) or Market (executes immediately against available liquidity)
enum class OrderType { 
    Limit, 
    Market 
};

struct Order {
    int id;                           // Unique identifier for the order
    Side side;                        // Buy or Sell
    double price;                     // Limit price (ignored for Market orders)
    int quantity;                     // Number of shares / units
    OrderType type = OrderType::Limit; // Order type (defaults to Limit)
};