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
#include "fft.hpp"

namespace algoforge {

class Backtester {
public:
    Backtester(double initial_cash,
               double zscore_entry  = 2.0,
               double zscore_exit   = 0.5,
               double slippage_pct  = 0.001,
               double stop_loss_pct = 0.05,
               std::string symbol   = "AAPL")
        : portfolio_(initial_cash)
        , initial_cash_(initial_cash)
        , zscore_entry_(zscore_entry)
        , zscore_exit_(zscore_exit)
        , slippage_(slippage_pct)
        , stop_loss_pct_(stop_loss_pct)
        , symbol_(symbol)
        , stats_(20)
        , fir_(21, 0.1)
        , candle_count_(0)
        , fft_period_(64.0)
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
                  << std::setw(10) << "FFT"
                  << std::setw(8)  << "Mode"
                  << "\n";
        std::cout << std::string(66, '-') << "\n";

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

        // FFT every 64 candles
        prices_.push_back(c.close);
        if (candle_count_ % 64 == 0 && (int)prices_.size() >= 64) {
            std::vector<double> window(prices_.end() - 64, prices_.end());
            auto spectrum = FFT::power_spectrum(window);
            auto dom_bin  = FFT::dominant_frequency(spectrum);
            fft_period_   = FFT::bin_to_period(dom_bin, 64);
        }

        if (!stats_.ready() || !fir_ready) return;

        double zscore   = stats_.zscore(c.close);
        double position = portfolio_.position(c.symbol);
        double equity   = portfolio_.equity(c.symbol, c.close);

        perf_.record_equity(equity);

        // Stop loss check
        if (position > 0) {
            double entry_p  = portfolio_.entry_price(c.symbol);
            double pct_chg  = (c.close - entry_p) / entry_p;
            if (pct_chg <= -stop_loss_pct_) {
                double sell_price = c.close * (1 - slippage_);
                std::cout << "  ⛔ STOP LOSS @ " << c.close
                          << " loss=" << pct_chg * 100 << "%\n";
                portfolio_.sell(c.symbol, sell_price, position, c.timestamp);
                return;
            }
        }

        // FFT trend filter
        bool is_trending = fft_period_ >= 32.0;  // was 48, now 32

        std::cout << std::setw(6)  << candle_count_
                  << std::setw(10) << c.close
                  << std::setw(10) << zscore
                  << std::setw(10) << position
                  << std::setw(12) << equity
                  << std::setw(10) << fft_period_
                  << std::setw(8)  << (is_trending ? "TREND" : "CYCLE")
                  << "\n";

        // Mean reversion — only when cycling
        if (!is_trending && zscore <= -zscore_entry_ && position == 0) {
            double buy_price = c.close * (1 + slippage_);
            double qty = std::floor(portfolio_.cash() / buy_price * 0.95);
            if (qty > 0)
                portfolio_.buy(c.symbol, buy_price, qty, c.timestamp);
        }
        else if (position > 0 && zscore >= -zscore_exit_) {
            double sell_price = c.close * (1 - slippage_);
            portfolio_.sell(c.symbol, sell_price, position, c.timestamp);
        }
        else if (position > 0 && zscore >= zscore_entry_) {
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

    double              initial_cash_;
    double              zscore_entry_;
    double              zscore_exit_;
    double              slippage_;
    double              stop_loss_pct_;
    std::string         symbol_;
    int                 candle_count_;
    double              fft_period_;
    std::vector<double> prices_;
    double              last_price_     = 0.0;
    int64_t             last_timestamp_ = 0;
};

} // namespace algoforge