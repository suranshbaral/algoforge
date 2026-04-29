#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <nlohmann/json.hpp>
#include "candle.hpp"
#include "backtester.hpp"

using namespace algoforge;
using json = nlohmann::json;

int main() {
    std::cout << "AlgoForge Backtester — Nokia (NOK)\n";
    std::cout << "===================================\n\n";

    // Load Nokia data from JSON
    std::ifstream file("/Users/suranshbaral/algoforge/nokia_data.json");
    if (!file.is_open()) {
        std::cerr << "Cannot open nokia_data.json\n";
        return 1;
    }

    json data;
    file >> data;

    std::vector<Candle> candles;
    for (const auto& d : data) {
        Candle c;
        c.symbol       = d["symbol"];
        c.open         = d["open"];
        c.high         = d["high"];
        c.low          = d["low"];
        c.close        = d["close"];
        c.volume       = d["volume"];
        c.timestamp    = d["timestamp"];
        c.interval_sec = d["interval_sec"];
        candles.push_back(c);
    }

    std::cout << "Loaded    : " << candles.size() << " daily candles\n";
    std::cout << "Symbol    : NOK\n";
    std::cout << "Period    : 2015-01-02 to 2023-12-29\n";
    std::cout << "Cash      : $10,000\n";
    std::cout << "Entry Z   : ±2.0\n";
    std::cout << "Exit Z    : 0.5\n";
    std::cout << "Slippage  : 0.1%\n\n";

    Backtester bt(10000.0, 2.0, 0.5, 0.001);
    bt.load(candles);
    bt.run();

    return 0;
}