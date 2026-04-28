#pragma once

#include <vector>
#include <numeric>
#include <stdexcept>
#include "candle.hpp"

namespace algoforge {

class FIRFilter {
public:
    // Simple low-pass FIR using windowed sinc
    // cutoff: 0.0 to 0.5 (fraction of sample rate)
    // taps: filter length — must be odd
    FIRFilter(int taps, double cutoff) : taps_(taps), cutoff_(cutoff) {
        if (taps % 2 == 0)
            throw std::invalid_argument("Taps must be odd");
        coeffs_ = design(taps, cutoff);
    }

    // Feed one price value, get filtered output
    // Returns true when enough samples are buffered
    bool update(double value, double& output) {
        buffer_.push_back(value);
        if ((int)buffer_.size() > taps_)
            buffer_.erase(buffer_.begin());

        if ((int)buffer_.size() < taps_)
            return false;

        // Convolution
        output = 0.0;
        for (int i = 0; i < taps_; i++)
            output += coeffs_[i] * buffer_[taps_ - 1 - i];

        return true;
    }

    // Apply to entire price series at once
    std::vector<double> apply(const std::vector<double>& prices) {
        std::vector<double> out;
        double val;
        for (double p : prices)
            if (update(p, val))
                out.push_back(val);
        return out;
    }

    const std::vector<double>& coefficients() const { return coeffs_; }

private:
    int taps_;
    double cutoff_;
    std::vector<double> coeffs_;
    std::vector<double> buffer_;

    // Windowed sinc design — Hamming window
    static std::vector<double> design(int taps, double cutoff) {
        std::vector<double> h(taps);
        int M = taps - 1;
        double sum = 0.0;

        for (int i = 0; i < taps; i++) {
            int n = i - M / 2;

            // Sinc function
            if (n == 0)
                h[i] = 2.0 * cutoff;
            else
                h[i] = std::sin(2.0 * M_PI * cutoff * n) / (M_PI * n);

            // Hamming window
            h[i] *= 0.54 - 0.46 * std::cos(2.0 * M_PI * i / M);
            sum += h[i];
        }

        // Normalize
        for (auto& v : h) v /= sum;
        return h;
    }
};

} // namespace algoforge