#pragma once

#include <vector>
#include <cmath>
#include <numeric>
#include <algorithm>
#include "fft.hpp"

namespace algoforge {

struct SpectralFeatures {
    double dominant_period;       // candles per cycle
    double band_power_low;        // periods 32-64  (slow trend)
    double band_power_mid;        // periods 8-32   (medium cycle)
    double band_power_high;       // periods 2-8    (noise)
    double spectral_entropy;      // 0=pure trend, 1=pure noise
    double trend_coherence;       // stability of dominant frequency
    double confidence;            // decays between FFT updates
    int    candles_since_update;  // how stale is this estimate

    bool is_fresh() const { return candles_since_update < 10; }
    bool is_stale() const { return candles_since_update > 48; }

    std::string regime_hint() const {
        if (spectral_entropy < 0.3 && band_power_low > 0.6)
            return "TRENDING";
        if (spectral_entropy < 0.5 && band_power_mid > 0.4)
            return "CYCLING";
        if (spectral_entropy > 0.7)
            return "NOISY";
        return "MIXED";
    }
};

class SpectralFeatureEngine {
public:
    SpectralFeatureEngine(int fft_window = 64,
                          double decay_lambda = 0.05)
        : fft_window_(fft_window)
        , decay_lambda_(decay_lambda)
        , candle_count_(0)
        , candles_since_update_(0)
        , prev_dominant_bin_(0)
        , coherence_sum_(0.0)
        , coherence_n_(0)
    {
        // Initialize with neutral features
        current_.dominant_period      = 64.0;
        current_.band_power_low       = 0.33;
        current_.band_power_mid       = 0.33;
        current_.band_power_high      = 0.33;
        current_.spectral_entropy     = 1.0;
        current_.trend_coherence      = 0.0;
        current_.confidence           = 0.0;
        current_.candles_since_update = 999;
    }

    // Call every candle — returns updated features
    const SpectralFeatures& update(double price) {
        candle_count_++;
        candles_since_update_++;
        prices_.push_back(price);

        // Decay confidence
        current_.confidence = std::exp(
            -decay_lambda_ * candles_since_update_);
        current_.candles_since_update = candles_since_update_;

        // Run FFT every fft_window_ candles
        if (candle_count_ % fft_window_ == 0 &&
            (int)prices_.size() >= fft_window_) {

            std::vector<double> window(
                prices_.end() - fft_window_, prices_.end());
            compute(window);
            candles_since_update_ = 0;
        }

        return current_;
    }

    const SpectralFeatures& features() const { return current_; }

private:
    void compute(const std::vector<double>& window) {
        int n = fft_window_;

        auto spectrum = FFT::power_spectrum(window);
        double total_power = 0.0;
        for (double p : spectrum) total_power += p;
        if (total_power == 0.0) return;

        // ── Dominant frequency ────────────────────────────
        std::size_t dom_bin = FFT::dominant_frequency(spectrum);
        current_.dominant_period = FFT::bin_to_period(dom_bin, n);

        // ── Band power ────────────────────────────────────
        // Low  band: bins 1-2   → periods 32-64
        // Mid  band: bins 2-8   → periods 8-32
        // High band: bins 8-32  → periods 2-8
        double low_power  = 0.0;
        double mid_power  = 0.0;
        double high_power = 0.0;

        for (int i = 1; i < (int)spectrum.size(); i++) {
            double period = (double)n / i;
            if (period >= 32.0)       low_power  += spectrum[i];
            else if (period >= 8.0)   mid_power  += spectrum[i];
            else                      high_power += spectrum[i];
        }

        // Normalize to sum to 1
        double band_total = low_power + mid_power + high_power;
        if (band_total > 0) {
            current_.band_power_low  = low_power  / band_total;
            current_.band_power_mid  = mid_power  / band_total;
            current_.band_power_high = high_power / band_total;
        }

        // ── Spectral entropy ──────────────────────────────
        // Shannon entropy over normalized power spectrum
        // Low  = energy concentrated = trending
        // High = energy spread = random/noisy
        double entropy = 0.0;
        for (std::size_t i = 1; i < spectrum.size(); i++) {
            double p = spectrum[i] / total_power;
            if (p > 1e-10)
                entropy -= p * std::log2(p);
        }
        // Normalize by max possible entropy (log2 of spectrum size)
        double max_entropy = std::log2(spectrum.size() - 1);
        current_.spectral_entropy = (max_entropy > 0)
            ? entropy / max_entropy : 1.0;

        // ── Trend coherence ───────────────────────────────
        // How stable is the dominant bin across updates?
        // 1.0 = same dominant bin as last time
        // 0.0 = completely different
        if (coherence_n_ > 0) {
            double bin_stability = 1.0 - std::abs(
                (double)dom_bin - (double)prev_dominant_bin_) /
                (spectrum.size() - 1);
            coherence_sum_ += bin_stability;
            coherence_n_++;
            current_.trend_coherence = coherence_sum_ / coherence_n_;
        } else {
            coherence_n_++;
            current_.trend_coherence = 0.5;
        }

        prev_dominant_bin_ = dom_bin;

        // Reset confidence
        current_.confidence           = 1.0;
        current_.candles_since_update = 0;
    }

    int    fft_window_;
    double decay_lambda_;
    int    candle_count_;
    int    candles_since_update_;

    std::size_t prev_dominant_bin_;
    double      coherence_sum_;
    int         coherence_n_;

    std::vector<double> prices_;
    SpectralFeatures    current_;
};

} // namespace algoforge