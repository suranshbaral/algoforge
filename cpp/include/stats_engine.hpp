#pragma once

#include <cstdint>
#include <cmath>
#include <deque>
#include <stdexcept>
#include <limits>
#include "candle.hpp"

namespace algoforge {

class StatsEngine {
public:
    StatsEngine()
        : n_(0)
        , mean_(0.0)
        , M2_(0.0)
        , min_(std::numeric_limits<double>::max())
        , max_(std::numeric_limits<double>::lowest())
    {}

    void update(const Candle& c) {
        double value = c.close;
        n_++;

        double delta  = value - mean_;
        mean_        += delta / n_;
        double delta2 = value - mean_;
        M2_          += delta * delta2;

        if (value < min_) min_ = value;
        if (value > max_) max_ = value;
    }

    double mean()     const {
        if (n_ == 0) throw std::runtime_error("No data");
        return mean_;
    }

    double variance() const {
        if (n_ < 2) throw std::runtime_error("Need at least 2 samples");
        return M2_ / (n_ - 1);
    }

    double stddev()   const { return std::sqrt(variance()); }

    double zscore(double value) const {
        double sd = stddev();
        if (sd == 0.0) throw std::runtime_error("Zero stddev");
        return (value - mean_) / sd;
    }

    double min()       const { return min_; }
    double max()       const { return max_; }
    int64_t count()    const { return n_; }

private:
    int64_t n_;
    double  mean_;
    double  M2_;
    double  min_;
    double  max_;
};

class RollingStatsEngine {
public:
    RollingStatsEngine(std::size_t window)
        : window_(window)
        , mean_(0.0)
        , M2_(0.0)
        , n_(0)
    {}

    void update(const Candle& c) {
        double value = c.close;

        window_data_.push_back(value);
        n_++;

        double delta  = value - mean_;
        mean_        += delta / n_;
        double delta2 = value - mean_;
        M2_          += delta * delta2;

        if (window_data_.size() > window_) {
            double old_value  = window_data_.front();
            window_data_.pop_front();
            n_--;

            double old_delta  = old_value - mean_;
            mean_            -= old_delta / n_;
            double old_delta2 = old_value - mean_;
            M2_              -= old_delta * old_delta2;
        }
    }

    double mean() const {
        if (n_ == 0) throw std::runtime_error("No data");
        return mean_;
    }

    double variance() const {
        if (n_ < 2) throw std::runtime_error("Need at least 2 samples");
        return M2_ / (n_ - 1);
    }

    double stddev()  const { return std::sqrt(variance()); }

    double zscore(double value) const {
        double sd = stddev();
        if (sd == 0.0) throw std::runtime_error("Zero stddev");
        return (value - mean_) / sd;
    }

    bool        ready() const { return window_data_.size() >= window_; }
    std::size_t count() const { return n_; }

private:
    std::size_t        window_;
    std::deque<double> window_data_;
    double             mean_;
    double             M2_;
    std::size_t        n_;
};

} // namespace algoforge