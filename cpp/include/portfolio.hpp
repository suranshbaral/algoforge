#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <iostream>
#include <iomanip>
#include <cmath>

namespace algoforge {

struct Trade {
    std::string symbol;
    double      entry_price;
    double      exit_price;
    double      quantity;
    int64_t     entry_time;
    int64_t     exit_time;
    double      pnl;
    int         holding_candles = 0;
};

class Portfolio {
public:
    Portfolio(double initial_cash)
        : cash_(initial_cash)
        , initial_cash_(initial_cash)
        , candle_count_(0)
    {}

    void increment_candle() { candle_count_++; }

    bool buy(const std::string& symbol, double price,
             double qty, int64_t timestamp) {
        double cost = price * qty;
        if (cost > cash_) {
            qty  = std::floor(cash_ / price);
            cost = price * qty;
        }
        if (qty <= 0) return false;

        cash_                 -= cost;
        positions_[symbol]    += qty;
        entry_prices_[symbol]  = price;
        entry_times_[symbol]   = timestamp;
        entry_candles_[symbol] = candle_count_;

        std::cout << "  BUY  " << qty << " " << symbol
                  << " @ " << std::fixed << std::setprecision(2)
                  << price << " cost=" << cost
                  << " cash=" << cash_ << "\n";
        return true;
    }

    bool sell(const std::string& symbol, double price,
              double qty, int64_t timestamp) {
        if (positions_[symbol] <= 0) return false;
        qty = std::min(qty, positions_[symbol]);

        double proceeds = price * qty;
        double pnl      = (price - entry_prices_[symbol]) * qty;

        cash_              += proceeds;
        positions_[symbol] -= qty;

        Trade t;
        t.symbol          = symbol;
        t.entry_price     = entry_prices_[symbol];
        t.exit_price      = price;
        t.quantity        = qty;
        t.entry_time      = entry_times_[symbol];
        t.exit_time       = timestamp;
        t.pnl             = pnl;
        t.holding_candles = candle_count_ - entry_candles_[symbol];
        trades_.push_back(t);

        std::cout << "  SELL " << qty << " " << symbol
                  << " @ " << std::fixed << std::setprecision(2)
                  << price << " pnl=" << pnl
                  << " holding=" << t.holding_candles << " candles"
                  << " cash=" << cash_ << "\n";
        return true;
    }

    double equity(const std::string& symbol, double current_price) const {
        auto it = positions_.find(symbol);
        if (it == positions_.end()) return cash_;
        return cash_ + it->second * current_price;
    }

    double cash()                         const { return cash_; }
    double position(const std::string& s) const {
        auto it = positions_.find(s);
        return it == positions_.end() ? 0.0 : it->second;
    }

    const std::vector<Trade>& trades() const { return trades_; }

    double total_pnl() const {
        double pnl = 0.0;
        for (const auto& t : trades_) pnl += t.pnl;
        return pnl;
    }
double entry_price(const std::string& s) const {
    auto it = entry_prices_.find(s);
    return it == entry_prices_.end() ? 0.0 : it->second;
}
private:
    double  cash_;
    double  initial_cash_;
    int     candle_count_;

    std::unordered_map<std::string, double>  positions_;
    std::unordered_map<std::string, double>  entry_prices_;
    std::unordered_map<std::string, int64_t> entry_times_;
    std::unordered_map<std::string, int>     entry_candles_;
    std::vector<Trade>                       trades_;
};

} // namespace algoforge