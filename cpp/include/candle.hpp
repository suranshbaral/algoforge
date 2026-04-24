#pragma once

#include <string>
#include <cstdint>

namespace algoforge {

struct Candle {
    std::string symbol;
    double open;
    double high;
    double low;
    double close;
    double volume;
    int64_t timestamp;
    int32_t interval_sec;
};

} // namespace algoforge