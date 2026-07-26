//OrderBook - Manager
#pragma once

#include <vector>
#include <optional>
//since it deals with order and trade, it needs to read their blueprints first
#include "order.h"
#include "trade.h"

//Why not Struct?
//Since, unlike a struct(deals with mostly data), here both data(active order) and behaviour(fns that act on data) are needed
class OrderBook {
public:
    // Adds a new order. Returns a Trade if it matched, or std::nullopt if it just rested.
    std::optional<Trade> addOrder(const Order& order);
    //std::optional<Trade> - this is a wrapper that holds a valid trade object or nothing
    //const Order& order - this passes the incoming order by const reference
    //const - incoming order wont be modified while reading it
    //& - pass by reference, no duplicate


private:
    //Two lists storing buy and sell orders respectively
    std::vector<Order> buyOrders;
    std::vector<Order> sellOrders;
};