#pragma once

#include <vector>
#include <cmath>
#include <numeric>
#include <algorithm>
#include <stdexcept>
#include "candle.hpp"

namespace algoforge {

struct StatFeatures {
    // Higher moments
    double skewness;        // asymmetry — positive = right tail
    double kurtosis;        // fat tails — >3 = leptokurtic
    double excess_kurtosis; // kurtosis - 3

    // Volatility
    double rolling_vol;     // standard deviation of returns
    double ewma_vol;        // exponentially weighted vol
    double vol_of_vol;      // volatility of volatility

    // Memory / trend
    double autocorr_1;      // lag-1 autocorrelation
    double autocorr_5;      // lag-5 autocorrelation
    double hurst;           // 0.5=random, >0.5=trending, <0.5=mean reverting

    // Complexity
    double entropy;         // Shannon entropy of return distribution
    double sample_entropy;  // regularity measure

    // Risk
    double var_95;          // Value at Risk 95%
    double cvar_95;         // Conditional VaR (Expected Shortfall)
    double max_drawdown;    // max drawdown in window

    // Drawdown state
    double current_drawdown;  // current drawdown from peak
    double drawdown_duration; // candles since last peak

    bool valid = false;
};

class StatisticalFeatureEngine {
public:
    StatisticalFeatureEngine(int window = 60,
                             double ewma_lambda = 0.94)
        : window_(window)
        , ewma_lambda_(ewma_lambda)
        , ewma_var_(0.0)
        , ewma_initialized_(false)
        , peak_price_(0.0)
        , drawdown_start_(0)
        , candle_n_(0)
    {}

    // Update with new candle — returns features when window is full
    bool update(const Candle& c, StatFeatures& out) {
        candle_n_++;
        prices_.push_back(c.close);

        // Track peak for drawdown
        if (c.close > peak_price_ || peak_price_ == 0.0) {
            peak_price_    = c.close;
            drawdown_start_ = candle_n_;
        }

        if ((int)prices_.size() > window_)
            prices_.erase(prices_.begin());

        if ((int)prices_.size() < window_)
            return false;

        // Compute returns
        std::vector<double> returns = compute_returns(prices_);

        // Update EWMA volatility
        double last_ret = returns.back();
        if (!ewma_initialized_) {
            ewma_var_         = last_ret * last_ret;
            ewma_initialized_ = true;
        } else {
            ewma_var_ = ewma_lambda_ * ewma_var_ +
                        (1.0 - ewma_lambda_) * last_ret * last_ret;
        }

        out.valid = true;

        // ── Higher moments ────────────────────────────────
        out.skewness        = compute_skewness(returns);
        out.kurtosis        = compute_kurtosis(returns);
        out.excess_kurtosis = out.kurtosis - 3.0;

        // ── Volatility ────────────────────────────────────
        out.rolling_vol = compute_std(returns);
        out.ewma_vol    = std::sqrt(ewma_var_);
        out.vol_of_vol  = compute_vol_of_vol(returns);

        // ── Autocorrelation ───────────────────────────────
        out.autocorr_1 = autocorrelation(returns, 1);
        out.autocorr_5 = autocorrelation(returns, 5);

        // ── Hurst exponent ────────────────────────────────
        out.hurst = hurst_exponent(prices_);

        // ── Entropy ───────────────────────────────────────
        out.entropy        = shannon_entropy(returns);
        out.sample_entropy = sample_entropy(returns);

        // ── Risk metrics ──────────────────────────────────
        out.var_95  = compute_var(returns, 0.05);
        out.cvar_95 = compute_cvar(returns, 0.05);
        out.max_drawdown = compute_max_drawdown(prices_);

        // ── Drawdown state ────────────────────────────────
        out.current_drawdown  = (peak_price_ - c.close) / peak_price_;
        out.drawdown_duration = candle_n_ - drawdown_start_;

        // Track vol-of-vol history
        vol_history_.push_back(out.rolling_vol);
        if ((int)vol_history_.size() > window_)
            vol_history_.erase(vol_history_.begin());

        return true;
    }

private:
    // ── Returns ───────────────────────────────────────────
    std::vector<double> compute_returns(const std::vector<double>& p) const {
        std::vector<double> r;
        for (int i = 1; i < (int)p.size(); i++)
            r.push_back((p[i] - p[i-1]) / p[i-1]);
        return r;
    }

    // ── Mean ──────────────────────────────────────────────
    double mean(const std::vector<double>& v) const {
        return std::accumulate(v.begin(), v.end(), 0.0) / v.size();
    }

    // ── Std dev ───────────────────────────────────────────
    double compute_std(const std::vector<double>& v) const {
        double m  = mean(v);
        double sq = 0.0;
        for (double x : v) sq += (x - m) * (x - m);
        return std::sqrt(sq / (v.size() - 1));
    }

    // ── Skewness ──────────────────────────────────────────
    // Third standardized moment
    double compute_skewness(const std::vector<double>& v) const {
        double m  = mean(v);
        double sd = compute_std(v);
        if (sd == 0.0) return 0.0;
        double s = 0.0;
        for (double x : v) s += std::pow((x - m) / sd, 3);
        return s / v.size();
    }

    // ── Kurtosis ──────────────────────────────────────────
    // Fourth standardized moment
    double compute_kurtosis(const std::vector<double>& v) const {
        double m  = mean(v);
        double sd = compute_std(v);
        if (sd == 0.0) return 3.0;
        double k = 0.0;
        for (double x : v) k += std::pow((x - m) / sd, 4);
        return k / v.size();
    }

    // ── Vol of vol ────────────────────────────────────────
    double compute_vol_of_vol(const std::vector<double>& returns) const {
        if (vol_history_.size() < 5) return 0.0;
        return compute_std(vol_history_);
    }

    // ── Autocorrelation at lag k ──────────────────────────
    double autocorrelation(const std::vector<double>& v, int lag) const {
        if ((int)v.size() <= lag) return 0.0;
        double m  = mean(v);
        double var = 0.0;
        for (double x : v) var += (x - m) * (x - m);
        if (var == 0.0) return 0.0;

        double cov = 0.0;
        for (int i = lag; i < (int)v.size(); i++)
            cov += (v[i] - m) * (v[i - lag] - m);

        return cov / var;
    }

    // ── Hurst exponent (R/S analysis) ─────────────────────
    // H > 0.5 → trending (persistent)
    // H = 0.5 → random walk
    // H < 0.5 → mean reverting (anti-persistent)
    double hurst_exponent(const std::vector<double>& p) const {
        if (p.size() < 20) return 0.5;

        std::vector<double> returns = compute_returns(p);
        int n = returns.size();

        std::vector<double> rs_values;
        std::vector<double> log_n_values;

        // Compute R/S for different sub-period lengths
        for (int sub = n / 4; sub <= n; sub += n / 4) {
            if (sub < 4) continue;

            double m   = mean(std::vector<double>(
                              returns.begin(), returns.begin() + sub));
            double var = 0.0;

            // Cumulative deviation from mean
            std::vector<double> cumdev(sub);
            double running = 0.0;
            for (int i = 0; i < sub; i++) {
                running   += returns[i] - m;
                cumdev[i]  = running;
                var       += (returns[i] - m) * (returns[i] - m);
            }

            double R = *std::max_element(cumdev.begin(), cumdev.end()) -
                       *std::min_element(cumdev.begin(), cumdev.end());
            double S = std::sqrt(var / sub);

            if (S > 0) {
                rs_values.push_back(std::log(R / S));
                log_n_values.push_back(std::log(sub));
            }
        }

        if (rs_values.size() < 2) return 0.5;

        // Linear regression of log(R/S) on log(n) → slope = Hurst
        double n_pts = rs_values.size();
        double sum_x = 0, sum_y = 0, sum_xy = 0, sum_xx = 0;
        for (int i = 0; i < (int)n_pts; i++) {
            sum_x  += log_n_values[i];
            sum_y  += rs_values[i];
            sum_xy += log_n_values[i] * rs_values[i];
            sum_xx += log_n_values[i] * log_n_values[i];
        }
        double denom = n_pts * sum_xx - sum_x * sum_x;
        if (denom == 0.0) return 0.5;
        return (n_pts * sum_xy - sum_x * sum_y) / denom;
    }

    // ── Shannon entropy of discretized returns ────────────
    double shannon_entropy(const std::vector<double>& v) const {
        if (v.empty()) return 0.0;

        // Discretize into 10 bins
        double mn = *std::min_element(v.begin(), v.end());
        double mx = *std::max_element(v.begin(), v.end());
        if (mx == mn) return 0.0;

        int bins = 10;
        std::vector<int> counts(bins, 0);
        for (double x : v) {
            int b = (int)((x - mn) / (mx - mn) * (bins - 1));
            counts[std::min(b, bins - 1)]++;
        }

        double entropy = 0.0;
        for (int c : counts) {
            if (c > 0) {
                double p = (double)c / v.size();
                entropy -= p * std::log2(p);
            }
        }
        return entropy / std::log2(bins); // normalize 0-1
    }

    // ── Sample entropy (regularity measure) ───────────────
    // Lower = more regular/predictable
    // Higher = more complex/random
    double sample_entropy(const std::vector<double>& v) const {
        if (v.size() < 10) return 0.0;
        int m = 2;
        double r = 0.2 * compute_std(v);
        if (r == 0.0) return 0.0;

        int A = 0, B = 0;
        int n = v.size();

        for (int i = 0; i < n - m; i++) {
            for (int j = i + 1; j < n - m; j++) {
                // Check m-length template match
                bool match_m = true;
                for (int k = 0; k < m; k++)
                    if (std::abs(v[i+k] - v[j+k]) >= r) {
                        match_m = false; break;
                    }
                if (match_m) {
                    B++;
                    // Check m+1 match
                    if (std::abs(v[i+m] - v[j+m]) < r)
                        A++;
                }
            }
        }

        if (B == 0) return 0.0;
        return -std::log((double)A / B);
    }

    // ── Value at Risk (historical simulation) ─────────────
    double compute_var(const std::vector<double>& returns,
                       double alpha) const {
        if (returns.empty()) return 0.0;
        std::vector<double> sorted = returns;
        std::sort(sorted.begin(), sorted.end());
        int idx = (int)(alpha * sorted.size());
        return sorted[idx];
    }

    // ── Conditional VaR (Expected Shortfall) ──────────────
    double compute_cvar(const std::vector<double>& returns,
                        double alpha) const {
        if (returns.empty()) return 0.0;
        std::vector<double> sorted = returns;
        std::sort(sorted.begin(), sorted.end());
        int cutoff = (int)(alpha * sorted.size());
        if (cutoff == 0) return sorted[0];
        double sum = 0.0;
        for (int i = 0; i < cutoff; i++) sum += sorted[i];
        return sum / cutoff;
    }

    // ── Max drawdown in price window ──────────────────────
    double compute_max_drawdown(const std::vector<double>& p) const {
        double peak  = p[0];
        double max_dd = 0.0;
        for (double x : p) {
            if (x > peak) peak = x;
            double dd = (peak - x) / peak;
            if (dd > max_dd) max_dd = dd;
        }
        return max_dd;
    }

    int    window_;
    double ewma_lambda_;
    double ewma_var_;
    bool   ewma_initialized_;
    double peak_price_;
    int    drawdown_start_;
    int    candle_n_;

    std::vector<double> prices_;
    std::vector<double> vol_history_;
};

} // namespace algoforge