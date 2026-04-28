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

using namespace algoforge;

int main() {
    std::cout << "AlgoForge engine starting...\n" << std::flush;

    zmq::context_t ctx(1);
    CandleGenerator    gen("AAPL", 180.0, 0.5, 60, 42);
    RingBuffer<128>    buffer;
    CandlePublisher    publisher(ctx, "tcp://*:5555");
    RollingStatsEngine rolling_stats(20);

    // Low-pass FIR — cutoff at 0.1 (filters out high frequency noise)
    // 21 taps — good balance between smoothing and lag
    FIRFilter fir(21, 0.1);

    std::vector<double> prices;
    int count = 0;

    std::cout << std::fixed << std::setprecision(4);
    std::cout << std::setw(6)  << "N"
              << std::setw(10) << "Raw"
              << std::setw(10) << "Filtered"
              << std::setw(10) << "Noise"
              << std::setw(10) << "Z-Score"
              << "\n";
    std::cout << std::string(46, '-') << "\n";

    while (true) {
        Candle c = gen.next();
        buffer.push(c);
        publisher.publish(c);
        rolling_stats.update(c);
        prices.push_back(c.close);
        count++;

        // FIR filter output
        double filtered;
        bool ready = fir.update(c.close, filtered);

        if (ready && rolling_stats.ready()) {
            double noise   = c.close - filtered;
            double zscore  = rolling_stats.zscore(c.close);

            std::cout << std::setw(6)  << count
                      << std::setw(10) << c.close
                      << std::setw(10) << filtered
                      << std::setw(10) << noise
                      << std::setw(10) << zscore
                      << "\n" << std::flush;
        }

        // FFT every 64 candles
        if (count % 64 == 0 && (int)prices.size() >= 64) {
            std::vector<double> window(prices.end() - 64, prices.end());
            auto spectrum = FFT::power_spectrum(window);
            auto dom_bin  = FFT::dominant_frequency(spectrum);
            auto period   = FFT::bin_to_period(dom_bin, 64);

            std::cout << "\n=== FFT @ candle " << count
                      << " | dominant period=" << period << " candles ===\n\n";
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    return 0;
}