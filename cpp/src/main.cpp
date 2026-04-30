#include <iostream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <chrono>
#include <thread>
#include <cmath>
#include "candle.hpp"
#include "ring_buffer.hpp"
#include "candle_generator.hpp"
#include "candle_publisher.hpp"
#include "stats_engine.hpp"
#include "fft.hpp"
#include "fir_filter.hpp"
#include "regime_fsm.hpp"
#include "spectral_features.hpp"
#include "statistical_features.hpp"

using namespace algoforge;

struct NoiseTracker {
    int    n    = 0;
    double mean = 0.0;
    double M2   = 0.0;

    void update(double v) {
        n++;
        double d  = v - mean;
        mean     += d / n;
        M2       += d * (v - mean);
    }

    double stddev() const {
        return n < 2 ? 1.0 : std::sqrt(M2 / (n - 1));
    }

    double zscore(double v) const {
        double sd = stddev();
        return sd == 0.0 ? 0.0 : (v - mean) / sd;
    }
};

int main() {
    std::cout << "AlgoForge engine starting...\n" << std::flush;

    zmq::context_t           ctx(1);
    CandleGenerator          gen("AAPL", 180.0, 0.5, 60, 42);
    RingBuffer<128>          buffer;
    CandlePublisher          publisher(ctx, "tcp://*:5555");
    RollingStatsEngine       rolling_stats(20);
    FIRFilter                fir(21, 0.1);
    RegimeFSM                fsm(3, 10, 2.0, 1.5);
    NoiseTracker             noise_tracker;
    SpectralFeatureEngine    spectral(64, 0.05);
    StatisticalFeatureEngine stat_engine(60, 0.94);

    double fft_period = 64.0;
    int    count      = 0;

    std::cout << std::fixed << std::setprecision(3);
    std::cout << std::setw(5)  << "N"
              << std::setw(9)  << "Close"
              << std::setw(9)  << "Z"
              << std::setw(9)  << "Noise"
              << std::setw(9)  << "NoiseZ"
              << std::setw(14) << "FSM"
              << std::setw(10) << "Entropy"
              << std::setw(10) << "Regime"
              << "\n";
    std::cout << std::string(75, '-') << "\n";

    while (true) {
        Candle c = gen.next();
        buffer.push(c);
        publisher.publish(c);
        rolling_stats.update(c);
        count++;

        double filtered = 0.0;
        double noise    = 0.0;
        double zscore   = 0.0;
        double noise_z  = 0.0;

        bool fir_ready   = fir.update(c.close, filtered);
        bool stats_ready = rolling_stats.ready();

        // Spectral engine — every candle
        const SpectralFeatures& sf = spectral.update(c.close);
        fft_period = sf.dominant_period;

        // Statistical features — every candle
        StatFeatures stat_f;
        bool stat_ready = stat_engine.update(c, stat_f);

        if (fir_ready && stats_ready) {
            noise   = c.close - filtered;
            zscore  = rolling_stats.zscore(c.close);
            noise_tracker.update(noise);
            noise_z = noise_tracker.zscore(noise);

            // Update FSM
            bool changed = fsm.update(c.close, zscore, noise,
                                      noise_z, fft_period, c.timestamp, sf);

            std::cout << std::setw(5)  << count
                      << std::setw(9)  << c.close
                      << std::setw(9)  << zscore
                      << std::setw(9)  << noise
                      << std::setw(9)  << noise_z
                      << std::setw(14) << fsm.fsm_state_str()
                      << std::setw(10) << sf.spectral_entropy
                      << std::setw(10) << sf.regime_hint()
                      << "\n" << std::flush;

            // Regime transition
            if (changed) {
                std::cout << "\n╔══ REGIME TRANSITION @ candle "
                          << count << " ══╗\n"
                          << fsm.state().summary()
                          << "╚════════════════════════════════╝\n\n";
            }

            // Spectral update block
            if (sf.candles_since_update == 0) {
                std::cout << "\n=== Spectral Update @ candle " << count << " ===\n"
                          << "  Dominant period  : " << sf.dominant_period  << "\n"
                          << "  Band low         : " << sf.band_power_low   << "\n"
                          << "  Band mid         : " << sf.band_power_mid   << "\n"
                          << "  Band high        : " << sf.band_power_high  << "\n"
                          << "  Spectral entropy : " << sf.spectral_entropy << "\n"
                          << "  Trend coherence  : " << sf.trend_coherence  << "\n"
                          << "  Regime hint      : " << sf.regime_hint()    << "\n"
                          << "  Confidence       : " << sf.confidence       << "\n\n";
            }

            // Statistical features block — print every 20 candles
            if (stat_ready && count % 20 == 0) {
                std::cout << "\n=== Statistical Features @ candle "
                          << count << " ===\n"
                          << std::fixed << std::setprecision(4)
                          << "  Skewness        : " << stat_f.skewness        << "\n"
                          << "  Kurtosis        : " << stat_f.kurtosis        << "\n"
                          << "  Excess Kurtosis : " << stat_f.excess_kurtosis << "\n"
                          << "  Rolling Vol     : " << stat_f.rolling_vol     << "\n"
                          << "  EWMA Vol        : " << stat_f.ewma_vol        << "\n"
                          << "  Autocorr lag-1  : " << stat_f.autocorr_1     << "\n"
                          << "  Autocorr lag-5  : " << stat_f.autocorr_5     << "\n"
                          << "  Hurst Exponent  : " << stat_f.hurst           << "\n"
                          << "  Shannon Entropy : " << stat_f.entropy         << "\n"
                          << "  Sample Entropy  : " << stat_f.sample_entropy  << "\n"
                          << "  VaR 95%         : " << stat_f.var_95          << "\n"
                          << "  CVaR 95%        : " << stat_f.cvar_95         << "\n"
                          << "  Max Drawdown    : " << stat_f.max_drawdown    << "\n"
                          << "  Current DD      : " << stat_f.current_drawdown<< "\n"
                          << "  DD Duration     : " << stat_f.drawdown_duration<< "\n\n";
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    return 0;
}