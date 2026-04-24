#pragma once

#include <random>
#include <string>
#include <cstdint>
#include "candle.hpp"
#include "ring_buffer.hpp"

namespace algoforge {

class CandleGenerator {
public:
    CandleGenerator(const std::string& symbol,
                double start_price,
                double volatility,
                int32_t interval_sec,
                uint64_t seed = std::random_device{}())
    : symbol_(symbol)
    , price_(start_price)
    , volatility_(volatility)
    , interval_sec_(interval_sec)
    , timestamp_(1700000000000)
    , rng_(seed)
    , dist_(0.0, volatility)
{}

    Candle next() {
        // Random walk — close is previous close + normal random move
        double move  = dist_(rng_);
        double open  = price_;
        double close = price_ + move;

        // Ensure price doesn't go negative
        close = std::max(close, 0.01);

        double high  = std::max(open, close) + std::abs(dist_(rng_)) * 0.5;
        double low   = std::min(open, close) - std::abs(dist_(rng_)) * 0.5;
        double volume = 500000 + std::abs(dist_(rng_)) * 100000;

        Candle c;
        c.symbol       = symbol_;
        c.open         = open;
        c.high         = high;
        c.low          = low;
        c.close        = close;
        c.volume       = volume;
        c.timestamp    = timestamp_;
        c.interval_sec = interval_sec_;

        // Advance state
        price_     = close;
        timestamp_ += interval_sec_ * 1000;

        return c;
    }

private:
    std::string symbol_;
    double price_;
    double volatility_;
    int32_t interval_sec_;
    int64_t timestamp_;

    std::mt19937_64 rng_;           // Mersenne Twister — we'll study this deeply in Monte Carlo
    std::normal_distribution<double> dist_;
};

} // namespace algoforge