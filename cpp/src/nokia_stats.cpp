#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <nlohmann/json.hpp>
#include "candle.hpp"
#include "statistical_features.hpp"
#include "spectral_features.hpp"
#include "stats_engine.hpp"
#include "fir_filter.hpp"

using namespace algoforge;
using json = nlohmann::json;

int main() {
    std::cout << "AlgoForge — Nokia Statistical Analysis\n";
    std::cout << "=======================================\n\n";

    // Load Nokia data
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

    std::cout << "Loaded " << candles.size()
              << " daily candles (2015-2024)\n\n";

    StatisticalFeatureEngine stat_engine(60, 0.94);
    SpectralFeatureEngine    spectral(64, 0.05);
    RollingStatsEngine       rolling(20);
    FIRFilter                fir(21, 0.1);

    // Store snapshots for summary
    std::vector<StatFeatures> snapshots;
    int count = 0;

    std::cout << std::fixed << std::setprecision(4);
    std::cout << std::setw(6)  << "N"
              << std::setw(10) << "Close"
              << std::setw(10) << "Hurst"
              << std::setw(10) << "Skew"
              << std::setw(10) << "ExKurt"
              << std::setw(10) << "EWMA_Vol"
              << std::setw(10) << "AutoC1"
              << std::setw(10) << "VaR95"
              << std::setw(10) << "CurrDD"
              << "\n";
    std::cout << std::string(86, '-') << "\n";

    for (const auto& c : candles) {
        count++;
        rolling.update(c);
        spectral.update(c.close);

        double filtered;
        fir.update(c.close, filtered);

        StatFeatures sf;
        bool ready = stat_engine.update(c, sf);

        if (!ready) continue;

        // Print every 20 candles
        if (count % 20 == 0) {
            std::cout << std::setw(6)  << count
                      << std::setw(10) << c.close
                      << std::setw(10) << sf.hurst
                      << std::setw(10) << sf.skewness
                      << std::setw(10) << sf.excess_kurtosis
                      << std::setw(10) << sf.ewma_vol
                      << std::setw(10) << sf.autocorr_1
                      << std::setw(10) << sf.var_95
                      << std::setw(10) << sf.current_drawdown
                      << "\n";
        }

        snapshots.push_back(sf);
    }

    // ── Full dataset summary ──────────────────────────────
    if (snapshots.empty()) {
        std::cout << "Not enough data\n";
        return 1;
    }

    // Aggregate stats across all windows
    double avg_hurst    = 0, avg_skew  = 0;
    double avg_kurt     = 0, avg_vol   = 0;
    double avg_autocorr = 0, avg_var   = 0;
    double max_dd       = 0;
    double min_hurst    = 1.0, max_hurst = 0.0;

    for (const auto& s : snapshots) {
        avg_hurst    += s.hurst;
        avg_skew     += s.skewness;
        avg_kurt     += s.excess_kurtosis;
        avg_vol      += s.ewma_vol;
        avg_autocorr += s.autocorr_1;
        avg_var      += s.var_95;
        if (s.max_drawdown > max_dd)  max_dd   = s.max_drawdown;
        if (s.hurst < min_hurst)      min_hurst = s.hurst;
        if (s.hurst > max_hurst)      max_hurst = s.hurst;
    }

    int n = snapshots.size();
    avg_hurst    /= n;
    avg_skew     /= n;
    avg_kurt     /= n;
    avg_vol      /= n;
    avg_autocorr /= n;
    avg_var      /= n;

    std::cout << "\n╔══ NOKIA STATISTICAL SUMMARY (2015-2024) ════════╗\n";
    std::cout << "  Windows analyzed  : " << n                 << "\n";
    std::cout << "  ─────────────────────────────────────────────\n";
    std::cout << "  Avg Hurst         : " << avg_hurst         << "\n";
    std::cout << "  Min/Max Hurst     : " << min_hurst
              << " / "                    << max_hurst         << "\n";
    std::cout << "  Avg Skewness      : " << avg_skew          << "\n";
    std::cout << "  Avg Excess Kurt   : " << avg_kurt          << "\n";
    std::cout << "  Avg EWMA Vol      : " << avg_vol           << "\n";
    std::cout << "  Avg Autocorr(1)   : " << avg_autocorr      << "\n";
    std::cout << "  Avg VaR 95%       : " << avg_var           << "\n";
    std::cout << "  Max Drawdown seen : " << max_dd * 100
              << "%\n";
    std::cout << "  ─────────────────────────────────────────────\n";

    // Interpretation
    std::cout << "\n  Hurst interpretation:\n";
    if (avg_hurst > 0.55)
        std::cout << "  → TRENDING (H>" << avg_hurst
                  << ") — momentum strategies may work\n";
    else if (avg_hurst < 0.45)
        std::cout << "  → MEAN REVERTING (H<" << avg_hurst
                  << ") — mean reversion strategies may work\n";
    else
        std::cout << "  → RANDOM WALK (H≈0.5) — hard to predict\n";

    std::cout << "\n  Kurtosis interpretation:\n";
    if (avg_kurt > 0)
        std::cout << "  → FAT TAILS (excess kurt=" << avg_kurt
                  << ") — extreme moves more common than normal\n";
    else
        std::cout << "  → THIN TAILS (excess kurt=" << avg_kurt
                  << ") — moves close to normal distribution\n";

    std::cout << "\n  Autocorrelation interpretation:\n";
    if (avg_autocorr > 0.05)
        std::cout << "  → MOMENTUM — returns positively correlated\n";
    else if (avg_autocorr < -0.05)
        std::cout << "  → MEAN REVERSION — returns negatively correlated\n";
    else
        std::cout << "  → NO MEMORY — returns uncorrelated\n";

    std::cout << "╚═════════════════════════════════════════════════╝\n";

    return 0;
}