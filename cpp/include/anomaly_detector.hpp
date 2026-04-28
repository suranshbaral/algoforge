#pragma once

#include <cmath>
#include <string>
#include <vector>
#include "candle.hpp"
#include "stats_engine.hpp"

namespace algoforge {

struct AnomalyEvent {
    int64_t     timestamp;
    double      price;
    double      zscore;
    double      noise;
    double      noise_zscore;
    std::string type;      // "BULLISH_EXTREME", "BEARISH_EXTREME", "REGIME_SHIFT"
    int         candle_n;
};

class AnomalyDetector {
public:
    AnomalyDetector(double zscore_threshold = 2.0,
                    double noise_threshold  = 2.0)
        : zscore_threshold_(zscore_threshold)
        , noise_threshold_(noise_threshold)
        , prev_noise_(0.0)
        , prev_zscore_(0.0)
        , n_(0)
    {}

    // Returns true if anomaly detected
    bool update(const Candle& c,
                double zscore,
                double noise,
                AnomalyEvent& event) {
        n_++;
        noise_stats_.update_raw(noise);
        bool detected = false;

        // Need enough history for noise stats
        if (n_ < 10) {
            prev_noise_  = noise;
            prev_zscore_ = zscore;
            return false;
        }

        double noise_z = noise_stats_.zscore(noise);

        // Rule 1 — price z-score extreme
        bool price_extreme = std::abs(zscore) >= zscore_threshold_;

        // Rule 2 — noise spike
        bool noise_spike = std::abs(noise_z) >= noise_threshold_;

        // Rule 3 — regime shift: noise crosses zero with momentum
        bool regime_shift = (prev_noise_ > 0.3 && noise < -0.3) ||
                            (prev_noise_ < -0.3 && noise > 0.3);

        if (price_extreme || noise_spike || regime_shift) {
            event.timestamp   = c.timestamp;
            event.price       = c.close;
            event.zscore      = zscore;
            event.noise       = noise;
            event.noise_zscore = noise_z;
            event.candle_n    = n_;

            if (regime_shift)
                event.type = "REGIME_SHIFT";
            else if (zscore > 0)
                event.type = "BULLISH_EXTREME";
            else
                event.type = "BEARISH_EXTREME";

            detected = true;
        }

        prev_noise_  = noise;
        prev_zscore_ = zscore;
        return detected;
    }

    int events_detected() const { return events_; }

private:
    double zscore_threshold_;
    double noise_threshold_;
    double prev_noise_;
    double prev_zscore_;
    int    n_;
    int    events_ = 0;

    // Inner stats tracker for noise distribution
    struct NoiseStats {
        int    n = 0;
        double mean = 0.0;
        double M2   = 0.0;

        void update_raw(double v) {
            n++;
            double d  = v - mean;
            mean     += d / n;
            double d2 = v - mean;
            M2       += d * d2;
        }

        double variance() const {
            return n < 2 ? 1.0 : M2 / (n - 1);
        }

        double stddev() const {
            return std::sqrt(variance());
        }

        double zscore(double v) const {
            double sd = stddev();
            return sd == 0.0 ? 0.0 : (v - mean) / sd;
        }
    } noise_stats_;
};

} // namespace algoforge