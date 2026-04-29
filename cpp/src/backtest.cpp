#include <iostream>
#include <vector>
#include "candle.hpp"
#include "candle_generator.hpp"
#include "backtester.hpp"

using namespace algoforge;

int main() {
    std::cout << "AlgoForge Backtester\n";
    std::cout << "====================\n\n";

    // Generate 500 candles of synthetic data
    CandleGenerator gen("AAPL", 180.0, 0.5, 60, 42);

    std::vector<Candle> candles;
    for (int i = 0; i < 500; i++)
        candles.push_back(gen.next());

    std::cout << "Strategy  : Mean Reversion\n";
    std::cout << "Symbol    : AAPL\n";
    std::cout << "Candles   : 500\n";
    std::cout << "Cash      : $10,000\n";
    std::cout << "Entry Z   : ±2.0\n";
    std::cout << "Exit Z    : 0.5\n";
    std::cout << "Slippage  : 0.1%\n\n";

    Backtester bt(10000.0, 2.0, 0.5, 0.001);
    bt.load(candles);
    bt.run();

    return 0;
}