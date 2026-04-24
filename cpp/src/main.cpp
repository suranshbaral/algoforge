#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include "candle.hpp"
#include "ring_buffer.hpp"
#include "candle_generator.hpp"
#include "candle_publisher.hpp"
#include "stats_engine.hpp"

using namespace algoforge;

int main() {
    std::cout << "AlgoForge engine starting...\n\n";

    zmq::context_t ctx(1);

    CandleGenerator    gen("AAPL", 180.0, 0.5, 60, 42);
    RingBuffer<100>    buffer;
    CandlePublisher    publisher(ctx, "tcp://*:5555");
    StatsEngine        global_stats;
    RollingStatsEngine rolling_stats(20);  // 20-candle window

    std::cout << std::fixed << std::setprecision(4);
    std::cout << std::setw(6)  << "N"
              << std::setw(10) << "Close"
              << std::setw(12) << "Global-Z"
              << std::setw(12) << "Rolling-Z"
              << std::setw(12) << "Roll-Mean"
              << std::setw(12) << "Roll-Std"
              << "\n";
    std::cout << std::string(64, '-') << "\n";

    int count = 0;
    while (true) {
        Candle c = gen.next();
        buffer.push(c);
        publisher.publish(c);
        global_stats.update(c);
        rolling_stats.update(c);

        count++;

        if (count >= 2 && rolling_stats.ready()) {
            std::cout << std::setw(6)  << count
                      << std::setw(10) << c.close
                      << std::setw(12) << global_stats.zscore(c.close)
                      << std::setw(12) << rolling_stats.zscore(c.close)
                      << std::setw(12) << rolling_stats.mean()
                      << std::setw(12) << rolling_stats.stddev()
                      << "\n";
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    return 0;
}