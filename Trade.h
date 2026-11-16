#pragma once

#include <string>

struct Trade {
    int id = 0;
    std::string symbol;
    std::string type;
    double price = 0.0;
    int quantity = 0;
};