#pragma once

#include <vector>
#include <complex>
#include <cmath>
#include <stdexcept>

namespace algoforge {

const double PI = std::acos(-1.0);

class FFT {
public:
    using Complex = std::complex<double>;
    using ComplexVec = std::vector<Complex>;

    // In-place Cooley-Tukey radix-2 FFT
    static void forward(ComplexVec& data) {
        std::size_t n = data.size();

        if (n == 0) return;
        if ((n & (n - 1)) != 0)
            throw std::invalid_argument("FFT size must be power of 2");

        // Bit-reversal permutation
        for (std::size_t i = 1, j = 0; i < n; i++) {
            std::size_t bit = n >> 1;
            for (; j & bit; bit >>= 1)
                j ^= bit;
            j ^= bit;
            if (i < j) std::swap(data[i], data[j]);
        }

        // Cooley-Tukey butterfly
        for (std::size_t len = 2; len <= n; len <<= 1) {
            double angle = -2.0 * PI / len;
            Complex wlen(std::cos(angle), std::sin(angle));

            for (std::size_t i = 0; i < n; i += len) {
                Complex w(1.0, 0.0);
                for (std::size_t j = 0; j < len / 2; j++) {
                    Complex u = data[i + j];
                    Complex v = data[i + j + len/2] * w;
                    data[i + j]          = u + v;
                    data[i + j + len/2]  = u - v;
                    w *= wlen;
                }
            }
        }
    }

    // Compute power spectrum from real price data
    static std::vector<double> power_spectrum(const std::vector<double>& prices) {
        std::size_t n = prices.size();

        if ((n & (n - 1)) != 0)
            throw std::invalid_argument("Input size must be power of 2");

        // Convert to complex
        ComplexVec data(n);
        for (std::size_t i = 0; i < n; i++)
            data[i] = Complex(prices[i], 0.0);

        forward(data);

        // Power spectrum — only first half is meaningful (Nyquist)
        std::vector<double> spectrum(n / 2);
        for (std::size_t i = 0; i < n / 2; i++)
            spectrum[i] = std::norm(data[i]) / n;

        return spectrum;
    }

    // Find dominant frequency bin
    static std::size_t dominant_frequency(const std::vector<double>& spectrum) {
        std::size_t peak = 0;
        // Skip bin 0 — that's the DC component (mean price level)
        for (std::size_t i = 1; i < spectrum.size(); i++)
            if (spectrum[i] > spectrum[peak] || peak == 0)
                peak = i;
        return peak;
    }

    // Convert bin index to period in candles
    static double bin_to_period(std::size_t bin, std::size_t n) {
        if (bin == 0) return std::numeric_limits<double>::infinity();
        return static_cast<double>(n) / bin;
    }
};

} // namespace algoforge