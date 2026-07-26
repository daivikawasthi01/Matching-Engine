//OrderBook - Manager
#pragma once

#include <vector>
//since it deals with order and trade, it needs to read their blueprints first
#include "order.h"
#include "trade.h"

//Why not Struct?
//Since, unlike a struct(deals with mostly data), here both data(active order) and behaviour(fns that act on data) are needed
class OrderBook {
public:
    // Adds a new order. Returns a Trade if it matched, or std::nullopt if it just rested.
    std::vector<Trade> addOrder(const Order& order);
    //std::vector allows an incoming order to execute multiple resting orders.
    //partial matching is allowed.
    //empty vector -> no matches and order simply rested

    bool cancelOrder(int orderId);
    //look up an order by its unique id and remove it from book

private:
    //Two lists storing buy and sell orders respectively
    std::vector<Order> buyOrders;
    std::vector<Order> sellOrders;
};