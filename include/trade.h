//How a trade receipt looks like
//When a buy order and a sell order match, a transaction happens

#pragma once


struct Trade {
    int buyOrderId; //ID of buyer
    int sellOrderId; //ID of seller
    double price; //The price at which they agreed to deal
    int quantity; //How many units were actually exchanged
};