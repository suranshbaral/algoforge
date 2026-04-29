#pragma once

#include <vector>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <iostream>
#include <iomanip>

namespace algoforge {

class PerformanceEngine {
public:
    void record_equity(double equity) {
        equity_curve_.push_back(equity);
    }

    // Sharpe ratio — risk adjusted return
    double sharpe(double risk_free = 0.0) const {
        if (equity_curve_.size() < 2) return 0.0;
        auto returns = compute_returns();
        double mean  = compute_mean(returns);
        double sd    = compute_std(returns);
        if (sd == 0.0) return 0.0;
        return (mean - risk_free) / sd * std::sqrt(252.0);
    }

    // Sortino — only penalizes downside volatility
    double sortino(double risk_free = 0.0) const {
        if (equity_curve_.size() < 2) return 0.0;
        auto returns = compute_returns();
        double mean  = compute_mean(returns);

        std::vector<double> downside;
        for (double r : returns)
            if (r < risk_free) downside.push_back(r);

        if (downside.empty()) return std::numeric_limits<double>::infinity();
        double down_std = compute_std(downside);
        if (down_std == 0.0) return 0.0;
        return (mean - risk_free) / down_std * std::sqrt(252.0);
    }

    // Max drawdown
    double max_drawdown() const {
        if (equity_curve_.empty()) return 0.0;
        double peak    = equity_curve_[0];
        double max_dd  = 0.0;
        for (double e : equity_curve_) {
            if (e > peak) peak = e;
            double dd = (peak - e) / peak;
            if (dd > max_dd) max_dd = dd;
        }
        return max_dd;
    }

    // Win rate
    double win_rate(const std::vector<double>& pnls) const {
        if (pnls.empty()) return 0.0;
        int wins = std::count_if(pnls.begin(), pnls.end(),
                                  [](double p){ return p > 0; });
        return (double)wins / pnls.size();
    }

    void print_summary(const std::vector<double>& trade_pnls,
                       double initial_cash,
                       double final_equity) const {
        std::cout << "\n╔══ BACKTEST RESULTS ══════════════════╗\n";
        std::cout << std::fixed << std::setprecision(4);
        std::cout << "  Initial Cash   : " << initial_cash    << "\n";
        std::cout << "  Final Equity   : " << final_equity    << "\n";
        std::cout << "  Total Return   : "
                  << (final_equity/initial_cash - 1.0)*100 << "%\n";
        std::cout << "  Sharpe Ratio   : " << sharpe()        << "\n";
        std::cout << "  Sortino Ratio  : " << sortino()       << "\n";
        std::cout << "  Max Drawdown   : "
                  << max_drawdown()*100 << "%\n";
        std::cout << "  Total Trades   : " << trade_pnls.size() << "\n";
        std::cout << "  Win Rate       : "
                  << win_rate(trade_pnls)*100 << "%\n";
        std::cout << "╚══════════════════════════════════════╝\n";
    }

    const std::vector<double>& equity_curve() const { return equity_curve_; }

private:
    std::vector<double> equity_curve_;

    std::vector<double> compute_returns() const {
        std::vector<double> returns;
        for (std::size_t i = 1; i < equity_curve_.size(); i++)
            returns.push_back(
                (equity_curve_[i] - equity_curve_[i-1]) / equity_curve_[i-1]);
        return returns;
    }

    double compute_mean(const std::vector<double>& v) const {
        return std::accumulate(v.begin(), v.end(), 0.0) / v.size();
    }

    double compute_std(const std::vector<double>& v) const {
        double mean = compute_mean(v);
        double sq   = 0.0;
        for (double x : v) sq += (x - mean) * (x - mean);
        return std::sqrt(sq / (v.size() - 1));
    }
};

} // namespace algoforge