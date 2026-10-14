/**************************************************************************/
/* frequency_response.hpp                                                 */
/**************************************************************************/
/*                          This file is part of:                         */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                         https://sushisystems.io                        */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the PolyForm Noncommercial License 1.0.0 (the           */
/* "License"); you may not use this file except in compliance with the    */
/* License. You may obtain a copy of the License at                       */
/*                                                                        */
/*     https://polyformproject.org/licenses/noncommercial/1.0.0           */
/*                                                                        */
/* Noncommercial use is free. Commercial use requires a separate licence  */
/* from Sushi Systems; see COMMERCIAL.md. The software is provided        */
/* "as is", without warranty of any kind.                                 */
/**************************************************************************/

/**
 * @file frequency_response.hpp
 * @brief Defines the swept-sine magnitude-response measurement of a node.
 * @author Mustafa Garip
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include <SushiDSP/foundation/core/audio_node.hpp>
#include <SushiDSP/foundation/math/fft.hpp>
#include <SushiDSP/foundation/signal_measure/render.hpp.hpp>

namespace SushiDSP::measure
{
    /// @brief One magnitude-response reading returned by frequency_response().
    struct ResponsePoint
    {
            double frequency_hz;
            double magnitude_db;
    };

    /// @brief Returns a node's small-signal magnitude response by sine sweep and deconvolution.
    /// @param node The node under test, knobs already set.
    /// @param frequencies_hz Frequencies to report.
    /// @return One point per requested frequency.
    /// @see modules/foundation/signal_measure/README.md, "frequency_response.hpp", for the sweep and its measured error.
    inline std::vector<ResponsePoint> frequency_response(AudioNode& node, double sample_rate,
                                                         const std::vector<double>& frequencies_hz)
    {
        const double sweep_seconds = 2.0;
        const double f1 = 5.0;
        const double f2 = sample_rate / 2.0;
        const std::size_t sweep_length = static_cast<std::size_t>(sweep_seconds * sample_rate);
        const std::size_t total_length = sweep_length + static_cast<std::size_t>(sample_rate);
        const double rate = std::log(f2 / f1);
        constexpr float amplitude = 0.01f; // -40 dBFS

        std::vector<float> sweep(total_length, 0.0f);
        for (std::size_t n = 0; n < sweep_length; ++n)
        {
            const double t = static_cast<double>(n) / sample_rate;
            sweep[n] = amplitude *
                       static_cast<float>(std::sin(2.0 * M_PI * f1 * sweep_seconds / rate *
                                                   (std::exp(t * rate / sweep_seconds) - 1.0)));
        }

        std::uint32_t fft_size = 1;
        while (fft_size < 2 * total_length)
        {
            fft_size *= 2;
        }
        const Fft fft(fft_size);

        auto spectrum_of = [&](const std::vector<float>& signal, std::vector<float>& real,
                               std::vector<float>& imaginary)
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
            inverse[sweep_length - 1 - n] =
                sweep[n] * static_cast<float>(std::exp((t - sweep_seconds) * rate / sweep_seconds));
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
                product_real[k] =
                    signal_real[k] * inverse_real[k] - signal_imaginary[k] * inverse_imaginary[k];
                product_imaginary[k] =
                    signal_real[k] * inverse_imaginary[k] + signal_imaginary[k] * inverse_real[k];
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
            const std::uint32_t bin =
                static_cast<std::uint32_t>(std::lround(frequency * fft_size / sample_rate));
            const double reference_power =
                static_cast<double>(reference_real[bin]) * reference_real[bin] +
                static_cast<double>(reference_imaginary[bin]) * reference_imaginary[bin];
            const double measured_power =
                static_cast<double>(measured_real[bin]) * measured_real[bin] +
                static_cast<double>(measured_imaginary[bin]) * measured_imaginary[bin];
            points.push_back({frequency, 10.0 * std::log10(measured_power / reference_power)});
        }
        return points;
    }
} // namespace SushiDSP::measure
