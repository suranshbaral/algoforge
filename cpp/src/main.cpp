#include <iostream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <chrono>
#include <thread>
#include "candle.hpp"
#include "ring_buffer.hpp"
#include "candle_generator.hpp"
#include "candle_publisher.hpp"
#include "stats_engine.hpp"
#include "fft.hpp"
#include "fir_filter.hpp"
#include "regime_fsm.hpp"

using namespace algoforge;

// Inline noise z-score tracker
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

    zmq::context_t     ctx(1);
    CandleGenerator    gen("AAPL", 180.0, 0.5, 60, 42);
    RingBuffer<128>    buffer;
    CandlePublisher    publisher(ctx, "tcp://*:5555");
    RollingStatsEngine rolling_stats(20);
    FIRFilter          fir(21, 0.1);
    RegimeFSM          fsm(3, 10, 2.0, 1.5);
    NoiseTracker       noise_tracker;

    std::vector<double> prices;
    double fft_period = 64.0;
    int count = 0;

    std::cout << std::fixed << std::setprecision(3);
    std::cout << std::setw(5)  << "N"
              << std::setw(9)  << "Close"
              << std::setw(9)  << "Z"
              << std::setw(9)  << "Noise"
              << std::setw(9)  << "NoiseZ"
              << std::setw(14) << "FSM"
              << "\n";
    std::cout << std::string(55, '-') << "\n";

    while (true) {
        Candle c = gen.next();
        buffer.push(c);
        publisher.publish(c);
        rolling_stats.update(c);
        prices.push_back(c.close);
        count++;

        double filtered, noise = 0.0, zscore = 0.0, noise_z = 0.0;
        bool fir_ready   = fir.update(c.close, filtered);
        bool stats_ready = rolling_stats.ready();

        if (fir_ready && stats_ready) {
            noise   = c.close - filtered;
            zscore  = rolling_stats.zscore(c.close);
            noise_tracker.update(noise);
            noise_z = noise_tracker.zscore(noise);

            // Update FFT every 64 candles
            if (count % 64 == 0 && (int)prices.size() >= 64) {
                std::vector<double> window(prices.end()-64, prices.end());
                auto spectrum  = FFT::power_spectrum(window);
                auto dom_bin   = FFT::dominant_frequency(spectrum);
                fft_period     = FFT::bin_to_period(dom_bin, 64);
            }

            // Update FSM
            bool changed = fsm.update(c.close, zscore, noise,
                                      noise_z, fft_period, c.timestamp);

            std::cout << std::setw(5)  << count
                      << std::setw(9)  << c.close
                      << std::setw(9)  << zscore
                      << std::setw(9)  << noise
                      << std::setw(9)  << noise_z
                      << std::setw(14) << fsm.fsm_state_str()
                      << "\n" << std::flush;

            // Print full state on transition
            if (changed) {
                std::cout << "\n╔══ REGIME TRANSITION @ candle "
                          << count << " ══╗\n"
                          << fsm.state().summary()
                          << "╚════════════════════════════════╝\n\n";
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    return 0;
}