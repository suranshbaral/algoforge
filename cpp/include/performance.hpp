#pragma once

#include <vector>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <iostream>
#include <iomanip>
#include <limits>

namespace algoforge {

class PerformanceEngine {
public:
    void record_equity(double equity) {
        equity_curve_.push_back(equity);
    }

    double sharpe(double risk_free = 0.0) const {
        if (equity_curve_.size() < 2) return 0.0;
        auto returns = compute_returns();
        double mean  = compute_mean(returns);
        double sd    = compute_std(returns);
        if (sd == 0.0) return 0.0;
        return (mean - risk_free) / sd * std::sqrt(252.0);
    }

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

    double max_drawdown() const {
        if (equity_curve_.empty()) return 0.0;
        double peak   = equity_curve_[0];
        double max_dd = 0.0;
        for (double e : equity_curve_) {
            if (e > peak) peak = e;
            double dd = (peak - e) / peak;
            if (dd > max_dd) max_dd = dd;
        }
        return max_dd;
    }

    double win_rate(const std::vector<double>& pnls) const {
        if (pnls.empty()) return 0.0;
        int wins = std::count_if(pnls.begin(), pnls.end(),
                                  [](double p){ return p > 0; });
        return (double)wins / pnls.size();
    }

    double avg_win(const std::vector<double>& pnls) const {
        std::vector<double> wins;
        for (double p : pnls) if (p > 0) wins.push_back(p);
        if (wins.empty()) return 0.0;
        return compute_mean(wins);
    }

    double avg_loss(const std::vector<double>& pnls) const {
        std::vector<double> losses;
        for (double p : pnls) if (p < 0) losses.push_back(p);
        if (losses.empty()) return 0.0;
        return compute_mean(losses);
    }

    double largest_win(const std::vector<double>& pnls) const {
        if (pnls.empty()) return 0.0;
        return *std::max_element(pnls.begin(), pnls.end());
    }

    double largest_loss(const std::vector<double>& pnls) const {
        if (pnls.empty()) return 0.0;
        return *std::min_element(pnls.begin(), pnls.end());
    }

    // Profit factor = gross profit / gross loss
    double profit_factor(const std::vector<double>& pnls) const {
        double gross_profit = 0.0;
        double gross_loss   = 0.0;
        for (double p : pnls) {
            if (p > 0) gross_profit += p;
            else       gross_loss   += std::abs(p);
        }
        if (gross_loss == 0.0) return std::numeric_limits<double>::infinity();
        return gross_profit / gross_loss;
    }

    // Average holding period in candles
    double avg_holding_period(const std::vector<int>& holding_periods) const {
        if (holding_periods.empty()) return 0.0;
        double sum = 0.0;
        for (int h : holding_periods) sum += h;
        return sum / holding_periods.size();
    }

    void print_summary(const std::vector<double>& pnls,
                       const std::vector<int>&    holding_periods,
                       double initial_cash,
                       double final_equity) const {
        int wins   = std::count_if(pnls.begin(), pnls.end(),
                                    [](double p){ return p > 0; });
        int losses = std::count_if(pnls.begin(), pnls.end(),
                                    [](double p){ return p <= 0; });

        std::cout << "\n╔══ BACKTEST RESULTS ══════════════════════╗\n";
        std::cout << std::fixed << std::setprecision(4);
        std::cout << "  Initial Cash        : $" << initial_cash         << "\n";
        std::cout << "  Final Equity        : $" << final_equity         << "\n";
        std::cout << "  Total Return        : "
                  << (final_equity/initial_cash - 1.0)*100 << "%\n";
        std::cout << "  ─────────────────────────────────────\n";
        std::cout << "  Sharpe Ratio        : " << sharpe()              << "\n";
        std::cout << "  Sortino Ratio       : " << sortino()             << "\n";
        std::cout << "  Max Drawdown        : "
                  << max_drawdown()*100 << "%\n";
        std::cout << "  Profit Factor       : "
                  << profit_factor(pnls)                                 << "\n";
        std::cout << "  ─────────────────────────────────────\n";
        std::cout << "  Total Trades        : " << pnls.size()           << "\n";
        std::cout << "  Wins / Losses       : " << wins
                  << " / " << losses                                     << "\n";
        std::cout << "  Win Rate            : "
                  << win_rate(pnls)*100 << "%\n";
        std::cout << "  ─────────────────────────────────────\n";
        std::cout << "  Avg Win             : $" << avg_win(pnls)        << "\n";
        std::cout << "  Avg Loss            : $" << avg_loss(pnls)       << "\n";
        std::cout << "  Largest Win         : $" << largest_win(pnls)    << "\n";
        std::cout << "  Largest Loss        : $" << largest_loss(pnls)   << "\n";
        std::cout << "  Avg Holding Period  : "
                  << avg_holding_period(holding_periods)
                  << " candles\n";
        std::cout << "╚══════════════════════════════════════════╝\n";
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
        if (v.empty()) return 0.0;
        return std::accumulate(v.begin(), v.end(), 0.0) / v.size();
    }

    double compute_std(const std::vector<double>& v) const {
        if (v.size() < 2) return 0.0;
        double mean = compute_mean(v);
        double sq   = 0.0;
        for (double x : v) sq += (x - mean) * (x - mean);
        return std::sqrt(sq / (v.size() - 1));
    }
};

} // namespace algoforge