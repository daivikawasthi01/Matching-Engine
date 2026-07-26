//How an order would look like - The Building Block

//pragma means pragmatic information, it is a special preprocessor used to give extra instructions to your compiler. pragma once here means load the header file only one time during compilation, stopping duplicate errors.
//it stops compiler from getting confused if two diff files ask to read order.h
#pragma once

#include<optional>

//side only has two values buy or sell
enum class Side { Buy, Sell };

//struct - user defined datatype that groups related variables of diff data types together. It is a blueprint. It applies to both class and object(its instance)
struct Order {
    int id; //unique number identifying the order
    Side side; //whether its a buy or sell
    double price; //how much money per unit
    int quantity; //how many units/shares
};