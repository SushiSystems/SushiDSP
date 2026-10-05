/****************************************************************/
/* Spectrum.hpp                                                 */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "SushiDSP/math/Fft.hpp"
#include "measure/Window.hpp"

namespace SushiDSP::measure
{
    inline constexpr std::uint32_t kFftSize = 65536;

    /// @brief Nearest frequency whose period divides fft_size exactly, so a windowed FFT of
    /// that tone puts all its energy at one bin's centre.
    /// @param frequency_hz Requested frequency.
    /// @param sample_rate Sample rate.
    /// @param fft_size Analysis length.
    /// @return The snapped frequency in Hz.
    inline double bin_exact_frequency(double frequency_hz, double sample_rate, std::uint32_t fft_size)
    {
        return std::round(frequency_hz * fft_size / sample_rate) * sample_rate / fft_size;
    }

    /// @brief Bin index of a frequency at kFftSize.
    /// @param frequency_hz Frequency.
    /// @param sample_rate Sample rate.
    /// @return Rounded bin index.
    inline std::uint32_t bin_of(double frequency_hz, double sample_rate)
    {
        return static_cast<std::uint32_t>(std::lround(frequency_hz * kFftSize / sample_rate));
    }

    /// @brief Blackman-Harris windowed magnitude spectrum of signal[offset, offset + kFftSize).
    /// @param signal Input, at least offset + kFftSize samples.
    /// @param offset First sample analysed (skip a node's settling time).
    /// @return kFftSize/2 + 1 magnitudes in dB (unnormalised; use differences between bins).
    inline std::vector<double> magnitude_spectrum_db(const std::vector<float>& signal, std::size_t offset)
    {
        static const std::vector<float> window = blackman_harris(kFftSize);
        static const Fft fft(kFftSize);
        std::vector<float> frame(kFftSize), real(kFftSize), imaginary(kFftSize);
        for (std::uint32_t i = 0; i < kFftSize; ++i)
        {
            frame[i] = signal[offset + i] * window[i];
        }
        fft.forward(frame.data(), real.data(), imaginary.data());
        std::vector<double> magnitude(kFftSize / 2 + 1);
        for (std::uint32_t bin = 0; bin < magnitude.size(); ++bin)
        {
            const double power = static_cast<double>(real[bin]) * real[bin] + static_cast<double>(imaginary[bin]) * imaginary[bin];
            magnitude[bin] = 10.0 * std::log10(std::max(power, 1e-300));
        }
        return magnitude;
    }

    /// @brief Highest magnitude within +-half_width bins of centre (the window's main lobe).
    /// @param spectrum From magnitude_spectrum_db.
    /// @param centre Bin index.
    /// @param half_width Bins each side to search; 4 covers a Blackman-Harris main lobe.
    /// @return Peak magnitude in dB.
    inline double peak_near(const std::vector<double>& spectrum, std::uint32_t centre, std::uint32_t half_width = 4)
    {
        const std::uint32_t first = centre > half_width ? centre - half_width : 0;
        const std::uint32_t last = std::min<std::uint32_t>(centre + half_width, static_cast<std::uint32_t>(spectrum.size() - 1));
        double peak = -300.0;
        for (std::uint32_t bin = first; bin <= last; ++bin)
        {
            peak = std::max(peak, spectrum[bin]);
        }
        return peak;
    }
}
