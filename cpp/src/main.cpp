#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <vector>
#include "candle.hpp"
#include "ring_buffer.hpp"
#include "candle_generator.hpp"
#include "candle_publisher.hpp"
#include "stats_engine.hpp"
#include "fft.hpp"

using namespace algoforge;

int main() {
    std::cout << "AlgoForge engine starting...\n\n";

    zmq::context_t ctx(1);

    CandleGenerator    gen("AAPL", 180.0, 0.5, 60, 42);
    RingBuffer<128>    buffer;   // power of 2
    CandlePublisher    publisher(ctx, "tcp://*:5555");
    RollingStatsEngine rolling_stats(20);

    std::vector<double> prices;
    int count = 0;

    std::cout << std::fixed << std::setprecision(4);

    while (true) {
        Candle c = gen.next();
        buffer.push(c);
        publisher.publish(c);
        rolling_stats.update(c);
        prices.push_back(c.close);
        count++;
        std::cout << "Candle " << count << " C=" << c.close << "\n" << std::flush;

        // Run FFT every 64 candles
        if (count % 64 == 0) {
            // Take last 64 prices
            std::vector<double> window(prices.end() - 64, prices.end());

            auto spectrum = FFT::power_spectrum(window);
            auto dom_bin  = FFT::dominant_frequency(spectrum);
            auto period   = FFT::bin_to_period(dom_bin, 64);

            std::cout << "\n=== FFT Analysis at candle " << count << " ===\n";
            std::cout << "Dominant frequency bin : " << dom_bin << "\n";
            std::cout << "Dominant period        : " << period << " candles\n";
            std::cout << "Rolling mean           : " << rolling_stats.mean() << "\n";
            std::cout << "Rolling stddev         : " << rolling_stats.stddev() << "\n";
            std::cout << "Rolling z-score        : " << rolling_stats.zscore(c.close) << "\n";

            // Print top 5 frequency bins
            std::cout << "\nTop 5 power bins:\n";
            std::vector<std::pair<double,std::size_t>> bins;
            for (std::size_t i = 1; i < spectrum.size(); i++)
                bins.push_back({spectrum[i], i});
            std::sort(bins.rbegin(), bins.rend());
            for (int i = 0; i < 5; i++) {
                std::cout << "  bin=" << std::setw(3) << bins[i].second
                          << "  period=" << std::setw(8)
                          << FFT::bin_to_period(bins[i].second, 64)
                          << " candles"
                          << "  power=" << bins[i].first << "\n";
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    return 0;
}