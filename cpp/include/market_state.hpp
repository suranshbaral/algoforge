#pragma once

#include <string>
#include <cstdint>
#include <sstream>

namespace algoforge {

enum class RegimeType {
    UNKNOWN,
    NEUTRAL,
    BULLISH_IMPULSE,
    BEARISH_IMPULSE,
    BULLISH_EXTREME,
    BEARISH_EXTREME,
    REGIME_SHIFT,
    TRENDING,
    CYCLING,
    NOISY
};

inline std::string regime_to_string(RegimeType r) {
    switch (r) {
        case RegimeType::UNKNOWN:         return "UNKNOWN";
        case RegimeType::NEUTRAL:         return "NEUTRAL";
        case RegimeType::BULLISH_IMPULSE: return "BULLISH_IMPULSE";
        case RegimeType::BEARISH_IMPULSE: return "BEARISH_IMPULSE";
        case RegimeType::BULLISH_EXTREME: return "BULLISH_EXTREME";
        case RegimeType::BEARISH_EXTREME: return "BEARISH_EXTREME";
        case RegimeType::REGIME_SHIFT:    return "REGIME_SHIFT";
        case RegimeType::TRENDING:        return "TRENDING";
        case RegimeType::CYCLING:         return "CYCLING";
        case RegimeType::NOISY:           return "NOISY";
        default:                          return "UNKNOWN";
    }
}

struct MarketState {
    // Core regime
    RegimeType  current;
    RegimeType  previous;
    double      confidence;
    int         duration;
    int         started_at;
    std::string transition_reason;

    // Statistical context
    double      avg_zscore;
    double      avg_noise;
    double      fft_period;
    int64_t     timestamp;

    // Running accumulators
    double      zscore_sum    = 0.0;
    double      noise_sum     = 0.0;
    int         confirm_count = 0;

    // Spectral context
    double      spectral_entropy    = 1.0;
    double      band_power_low      = 0.33;
    double      band_power_mid      = 0.33;
    double      band_power_high     = 0.33;
    double      spectral_confidence = 0.0;
    std::string spectral_regime     = "UNKNOWN";

    MarketState()
        : current(RegimeType::UNKNOWN)
        , previous(RegimeType::UNKNOWN)
        , confidence(0.0)
        , duration(0)
        , started_at(0)
        , avg_zscore(0.0)
        , avg_noise(0.0)
        , fft_period(64.0)
        , timestamp(0)
    {}

    std::string summary() const {
        std::ostringstream oss;
        oss << "State          : " << regime_to_string(current)   << "\n"
            << "Previous       : " << regime_to_string(previous)  << "\n"
            << "Confidence     : " << confidence                   << "\n"
            << "Duration       : " << duration << " candles\n"
            << "Started at     : candle " << started_at           << "\n"
            << "Avg Z-Score    : " << avg_zscore                  << "\n"
            << "Avg Noise      : " << avg_noise                   << "\n"
            << "FFT Period     : " << fft_period << " candles\n"
            << "Spectral       : " << spectral_regime
            << " entropy="        << spectral_entropy
            << " conf="           << spectral_confidence          << "\n"
            << "Band power     : "
            << "low="             << band_power_low
            << " mid="            << band_power_mid
            << " high="           << band_power_high              << "\n"
            << "Reason         : " << transition_reason           << "\n";
        return oss.str();
    }
};

} // namespace algoforge