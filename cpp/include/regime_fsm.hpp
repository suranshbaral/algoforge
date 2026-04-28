#pragma once

#include <cmath>
#include <sstream>
#include "market_state.hpp"
#include "stats_engine.hpp"

namespace algoforge {

class RegimeFSM {
public:
    enum class FSMState {
        WATCHING,
        TRIGGERED,
        COOLDOWN
    };

    RegimeFSM(int confirm_candles = 3,
              int cooldown_candles = 10,
              double zscore_threshold = 2.0,
              double noise_z_threshold = 1.5)
        : confirm_needed_(confirm_candles)
        , cooldown_needed_(cooldown_candles)
        , zscore_thresh_(zscore_threshold)
        , noise_z_thresh_(noise_z_threshold)
        , fsm_state_(FSMState::WATCHING)
        , confirm_count_(0)
        , cooldown_count_(0)
        , candle_n_(0)
        , prev_noise_(0.0)
        , state_changed_(false)
    {}

    // Call every candle — returns true if market state changed
    bool update(double price,
                double zscore,
                double noise,
                double noise_z,
                double fft_period,
                int64_t timestamp) {
        candle_n_++;
        state_changed_ = false;

        // Update noise stats for running averages
        noise_stats_.update_raw(noise);
        zscore_stats_.update_raw(zscore);

        // Accumulate into current state
        state_.duration++;
        state_.zscore_sum += zscore;
        state_.noise_sum  += noise;
        state_.avg_zscore  = state_.zscore_sum / state_.duration;
        state_.avg_noise   = state_.noise_sum  / state_.duration;
        state_.timestamp   = timestamp;

        // Determine candidate regime from current signals
        RegimeType candidate = classify(zscore, noise, noise_z);

        switch (fsm_state_) {

            case FSMState::WATCHING:
                if (candidate != RegimeType::NEUTRAL &&
                    candidate != state_.current) {
                    // Condition appeared — start confirmation
                    pending_        = candidate;
                    pending_reason_ = build_reason(zscore, noise, noise_z, fft_period);
                    confirm_count_  = 1;
                    fsm_state_      = FSMState::TRIGGERED;
                }
                break;

            case FSMState::TRIGGERED:
                if (classify(zscore, noise, noise_z) == pending_) {
                    confirm_count_++;
                    if (confirm_count_ >= confirm_needed_) {
                        // Confirmed — transition state
                        transition(pending_, pending_reason_, fft_period, timestamp);
                        fsm_state_     = FSMState::COOLDOWN;
                        cooldown_count_ = 0;
                        state_changed_ = true;
                    }
                } else {
                    // Condition broke — false alarm
                    fsm_state_     = FSMState::WATCHING;
                    confirm_count_ = 0;
                }
                break;

            case FSMState::COOLDOWN:
                cooldown_count_++;
                if (cooldown_count_ >= cooldown_needed_) {
                    fsm_state_ = FSMState::WATCHING;
                }
                break;
        }

        prev_noise_ = noise;
        return state_changed_;
    }

    const MarketState& state() const { return state_; }
    FSMState fsm_state() const { return fsm_state_; }
    int candle_n() const { return candle_n_; }

    std::string fsm_state_str() const {
        switch (fsm_state_) {
            case FSMState::WATCHING:  return "WATCHING";
            case FSMState::TRIGGERED: return "TRIGGERED(" +
                                      std::to_string(confirm_count_) +
                                      "/" +
                                      std::to_string(confirm_needed_) + ")";
            case FSMState::COOLDOWN:  return "COOLDOWN(" +
                                      std::to_string(cooldown_count_) +
                                      "/" +
                                      std::to_string(cooldown_needed_) + ")";
            default: return "UNKNOWN";
        }
    }

private:
    // Classify signals into a regime
    RegimeType classify(double zscore, double noise, double noise_z) {
        bool price_bull  = zscore  >=  zscore_thresh_;
        bool price_bear  = zscore  <= -zscore_thresh_;
        bool noise_spike = std::abs(noise_z) >= noise_z_thresh_;
        bool shift       = (prev_noise_ >  0.5 && noise < -0.5) ||
                           (prev_noise_ < -0.5 && noise >  0.5);

        if (shift)                        return RegimeType::REGIME_SHIFT;
        if (price_bull && noise_spike)    return RegimeType::BULLISH_EXTREME;
        if (price_bear && noise_spike)    return RegimeType::BEARISH_EXTREME;
        if (price_bull)                   return RegimeType::BULLISH_IMPULSE;
        if (price_bear)                   return RegimeType::BEARISH_IMPULSE;
        return RegimeType::NEUTRAL;
    }

    void transition(RegimeType next,
                    const std::string& reason,
                    double fft_period,
                    int64_t timestamp) {
        state_.previous          = state_.current;
        state_.current           = next;
        state_.confidence        = compute_confidence();
        state_.started_at        = candle_n_;
        state_.duration          = 0;
        state_.zscore_sum        = 0.0;
        state_.noise_sum         = 0.0;
        state_.avg_zscore        = 0.0;
        state_.avg_noise         = 0.0;
        state_.fft_period        = fft_period;
        state_.transition_reason = reason;
        state_.timestamp         = timestamp;
    }

    double compute_confidence() {
        // Confidence grows with confirmation count
        // normalized 0-1, capped at confirm_needed
        return std::min(1.0, (double)confirm_count_ / confirm_needed_);
    }

    std::string build_reason(double zscore, double noise,
                             double noise_z, double fft_period) {
        std::ostringstream oss;
        oss << "zscore="   << zscore
            << " noise="   << noise
            << " noise_z=" << noise_z
            << " fft_period=" << fft_period;
        return oss.str();
    }

    // Inner Welford tracker
    struct RunningStats {
        int    n    = 0;
        double mean = 0.0;
        double M2   = 0.0;

        void update_raw(double v) {
            n++;
            double d  = v - mean;
            mean     += d / n;
            M2       += d * (v - mean);
        }
    };

    int    confirm_needed_;
    int    cooldown_needed_;
    double zscore_thresh_;
    double noise_z_thresh_;

    FSMState fsm_state_;
    int      confirm_count_;
    int      cooldown_count_;
    int      candle_n_;
    double   prev_noise_;
    bool     state_changed_;

    RegimeType  pending_;
    std::string pending_reason_;

    MarketState  state_;
    RunningStats noise_stats_;
    RunningStats zscore_stats_;
};

} // namespace algoforge