/****************************************************************/
/* FrequencyResponse.hpp                                        */
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

#include "SushiDSP/core/IAudioNode.hpp"
#include "SushiDSP/math/Fft.hpp"
#include "measure/Render.hpp"

namespace SushiDSP::measure
{
    /// @brief One magnitude-response reading returned by frequency_response().
    struct ResponsePoint
    {
        double frequency_hz;
        double magnitude_db;
    };

    /// @brief Small-signal magnitude response by exponential sine sweep and deconvolution.
    ///
    /// Sweep 5 Hz -> sample_rate/2 over two seconds at -40 dBFS, followed by one second of
    /// silence for the tail. The sweep deliberately starts two octaves below the lowest
    /// frequency any caller reports: an ESS carries no energy below its own start frequency,
    /// and the abrupt onset at f1 leaves the first few bins of the deconvolved response
    /// contaminated by that truncation. Measured against an analytic 2nd-order Butterworth
    /// high-pass at 192 kHz, a sweep starting at 20 Hz reads 20 Hz as -27.250 dB against an
    /// analytic -25.552 dB (error -1.699 dB) for a 87 Hz corner, and -6.386 vs -3.010
    /// (error -3.375 dB) for a 20 Hz corner. Starting at 5 Hz instead brings both to
    /// -0.068 dB, and the worst error anywhere from 20 Hz up is 0.17 dB. The inverse filter is the time-reversed sweep with a 6 dB/octave
    /// amplitude envelope (Farina). The envelope rises along the original sweep's time axis,
    /// so it compensates the sweep's 1/f energy density: the low frequencies at the sweep's
    /// start are attenuated and the high frequencies at its end are not. Weighting it the
    /// other way round attenuates the highs twice over and deconvolves to a low-frequency
    /// blob rather than a delta. The linear impulse response is the circular convolution
    /// of output and inverse. Harmonic products land before the linear response and are
    /// excluded by keeping only the causal second. The stimulus itself is deconvolved the same
    /// way and used as the 0 dB reference.
    /// @param node The node under test, knobs already set.
    /// @param sample_rate Sample rate.
    /// @param frequencies_hz Frequencies to report.
    /// @return One point per requested frequency.
    inline std::vector<ResponsePoint> frequency_response(IAudioNode& node, double sample_rate, const std::vector<double>& frequencies_hz)
    {
        const double sweep_seconds = 2.0;
        const double f1 = 5.0;
        const double f2 = sample_rate / 2.0;
        const std::size_t sweep_length = static_cast<std::size_t>(sweep_seconds * sample_rate);
        const std::size_t total_length = sweep_length + static_cast<std::size_t>(sample_rate);
        const double rate = std::log(f2 / f1);
        constexpr float amplitude = 0.01f;   // -40 dBFS

        std::vector<float> sweep(total_length, 0.0f);
        for (std::size_t n = 0; n < sweep_length; ++n)
        {
            const double t = static_cast<double>(n) / sample_rate;
            sweep[n] = amplitude * static_cast<float>(std::sin(2.0 * M_PI * f1 * sweep_seconds / rate * (std::exp(t * rate / sweep_seconds) - 1.0)));
        }

        std::uint32_t fft_size = 1;
        while (fft_size < 2 * total_length)
        {
            fft_size *= 2;
        }
        const Fft fft(fft_size);

        auto spectrum_of = [&](const std::vector<float>& signal, std::vector<float>& real, std::vector<float>& imaginary)
        {
            std::vector<float> padded(fft_size, 0.0f);
            std::copy(signal.begin(), signal.end(), padded.begin());
            real.assign(fft_size, 0.0f);
            imaginary.assign(fft_size, 0.0f);
            fft.forward(padded.data(), real.data(), imaginary.data());
        };

        std::vector<float> inverse(total_length, 0.0f);
        for (std::size_t n = 0; n < sweep_length; ++n)
        {
            const double t = static_cast<double>(n) / sample_rate;
            inverse[sweep_length - 1 - n] = sweep[n] * static_cast<float>(std::exp((t - sweep_seconds) * rate / sweep_seconds));
        }
        std::vector<float> inverse_real, inverse_imaginary;
        spectrum_of(inverse, inverse_real, inverse_imaginary);

        auto impulse_of = [&](const std::vector<float>& signal)
        {
            std::vector<float> signal_real, signal_imaginary;
            spectrum_of(signal, signal_real, signal_imaginary);
            std::vector<float> product_real(fft_size), product_imaginary(fft_size);
            for (std::uint32_t k = 0; k < fft_size; ++k)
            {
                product_real[k] = signal_real[k] * inverse_real[k] - signal_imaginary[k] * inverse_imaginary[k];
                product_imaginary[k] = signal_real[k] * inverse_imaginary[k] + signal_imaginary[k] * inverse_real[k];
            }
            std::vector<float> impulse(fft_size);
            fft.inverse(product_real.data(), product_imaginary.data(), impulse.data());
            // The linear response begins at index sweep_length - 1; keep one second from there.
            std::vector<float> causal(static_cast<std::size_t>(sample_rate), 0.0f);
            for (std::size_t i = 0; i < causal.size(); ++i)
            {
                causal[i] = impulse[sweep_length - 1 + i];
            }
            return causal;
        };

        const auto reference = impulse_of(sweep);
        node.reset();
        const auto measured = impulse_of(render(node, sweep, sample_rate, 256));

        std::vector<float> reference_real, reference_imaginary, measured_real, measured_imaginary;
        spectrum_of(reference, reference_real, reference_imaginary);
        spectrum_of(measured, measured_real, measured_imaginary);

        std::vector<ResponsePoint> points;
        for (const double frequency : frequencies_hz)
        {
            const std::uint32_t bin = static_cast<std::uint32_t>(std::lround(frequency * fft_size / sample_rate));
            const double reference_power = static_cast<double>(reference_real[bin]) * reference_real[bin] + static_cast<double>(reference_imaginary[bin]) * reference_imaginary[bin];
            const double measured_power = static_cast<double>(measured_real[bin]) * measured_real[bin] + static_cast<double>(measured_imaginary[bin]) * measured_imaginary[bin];
            points.push_back({ frequency, 10.0 * std::log10(measured_power / reference_power) });
        }
        return points;
    }
}
