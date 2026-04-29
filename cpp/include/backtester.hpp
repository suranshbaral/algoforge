#pragma once

#include <queue>
#include <vector>
#include <string>
#include <iostream>
#include <iomanip>
#include "event.hpp"
#include "candle.hpp"
#include "portfolio.hpp"
#include "performance.hpp"
#include "stats_engine.hpp"
#include "fir_filter.hpp"

namespace algoforge {

class Backtester {
public:
    Backtester(double initial_cash,
               double zscore_entry  = 2.0,
               double zscore_exit   = 0.5,
               double slippage_pct  = 0.001,
               std::string symbol   = "AAPL")
        : portfolio_(initial_cash)
        , initial_cash_(initial_cash)
        , zscore_entry_(zscore_entry)
        , zscore_exit_(zscore_exit)
        , slippage_(slippage_pct)
        , symbol_(symbol)
        , stats_(20)
        , fir_(21, 0.1)
        , candle_count_(0)
    {}

    void load(const std::vector<Candle>& candles) {
        for (const auto& c : candles)
            events_.push(make_candle_event(c));
        std::cout << "Loaded " << candles.size() << " candles\n\n";
    }

    void run() {
        std::cout << std::fixed << std::setprecision(2);
        std::cout << std::setw(6)  << "N"
                  << std::setw(10) << "Close"
                  << std::setw(10) << "Z-Score"
                  << std::setw(10) << "Position"
                  << std::setw(12) << "Equity"
                  << "\n";
        std::cout << std::string(48, '-') << "\n";

        while (!events_.empty()) {
            Event e = events_.top();
            events_.pop();
            process(e);
        }

        // Close any open positions at last price
        if (portfolio_.position(symbol_) > 0) {
            portfolio_.sell(symbol_, last_price_,
                            portfolio_.position(symbol_),
                            last_timestamp_);
        }

        // Collect results
        std::vector<double> pnls;
        std::vector<int>    holding_periods;
        for (const auto& t : portfolio_.trades()) {
            pnls.push_back(t.pnl);
            holding_periods.push_back(t.holding_candles);
        }

        double final_equity = portfolio_.equity(symbol_, last_price_);
        perf_.print_summary(pnls, holding_periods, initial_cash_, final_equity);
    }

private:
    void process(const Event& e) {
        if (e.type != EventType::CANDLE) return;

        const Candle& c = e.candle;
        candle_count_++;
        portfolio_.increment_candle();
        last_price_     = c.close;
        last_timestamp_ = c.timestamp;

        stats_.update(c);
        double filtered;
        bool fir_ready = fir_.update(c.close, filtered);

        if (!stats_.ready() || !fir_ready) return;

        double zscore   = stats_.zscore(c.close);
        double position = portfolio_.position(c.symbol);
        double equity   = portfolio_.equity(c.symbol, c.close);

        perf_.record_equity(equity);

        std::cout << std::setw(6)  << candle_count_
                  << std::setw(10) << c.close
                  << std::setw(10) << zscore
                  << std::setw(10) << position
                  << std::setw(12) << equity
                  << "\n";

        // Mean reversion strategy
        if (zscore <= -zscore_entry_ && position == 0) {
            double buy_price = c.close * (1 + slippage_);
            double qty = std::floor(portfolio_.cash() / buy_price * 0.95);
            if (qty > 0)
                portfolio_.buy(c.symbol, buy_price, qty, c.timestamp);
        }
        else if (zscore >= -zscore_exit_ && position > 0) {
            double sell_price = c.close * (1 - slippage_);
            portfolio_.sell(c.symbol, sell_price, position, c.timestamp);
        }
        else if (zscore >= zscore_entry_ && position > 0) {
            double sell_price = c.close * (1 - slippage_);
            portfolio_.sell(c.symbol, sell_price, position, c.timestamp);
        }
    }

    std::priority_queue<Event,
                        std::vector<Event>,
                        std::greater<Event>> events_;

    Portfolio          portfolio_;
    PerformanceEngine  perf_;
    RollingStatsEngine stats_;
    FIRFilter          fir_;

    double      initial_cash_;
    double      zscore_entry_;
    double      zscore_exit_;
    double      slippage_;
    std::string symbol_;
    int         candle_count_;
    double      last_price_     = 0.0;
    int64_t     last_timestamp_ = 0;
};

} // namespace algoforge