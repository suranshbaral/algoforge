#pragma once

#include <cstdint>
#include <string>
#include "candle.hpp"

namespace algoforge {

enum class EventType {
    CANDLE,       // new candle arrived
    ORDER,        // order to be executed
    FILL,         // order filled
    SIGNAL,       // strategy signal generated
};

enum class OrderSide {
    BUY,
    SELL
};

enum class OrderType {
    MARKET,
    LIMIT
};

struct Event {
    EventType type;
    int64_t   timestamp;  // used for priority queue ordering

    // Candle event
    Candle candle;

    // Order event
    std::string symbol;
    OrderSide   side;
    OrderType   order_type;
    double      quantity;
    double      limit_price;  // only for LIMIT orders

    // Fill event
    double fill_price;
    double fill_qty;

    // Signal event
    double   signal_value;   // zscore or other signal
    std::string signal_name;

    // Priority queue ordering — earliest timestamp first
    bool operator>(const Event& other) const {
        return timestamp > other.timestamp;
    }
};

// Factory functions for clean event creation
inline Event make_candle_event(const Candle& c) {
    Event e;
    e.type      = EventType::CANDLE;
    e.timestamp = c.timestamp;
    e.candle    = c;
    return e;
}

inline Event make_order_event(const std::string& symbol,
                               OrderSide side,
                               double qty,
                               int64_t timestamp,
                               OrderType otype = OrderType::MARKET,
                               double limit_price = 0.0) {
    Event e;
    e.type        = EventType::ORDER;
    e.timestamp   = timestamp;
    e.symbol      = symbol;
    e.side        = side;
    e.order_type  = otype;
    e.quantity    = qty;
    e.limit_price = limit_price;
    return e;
}

} // namespace algoforge