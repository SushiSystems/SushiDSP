/**************************************************************************/
/* IrLoader.cpp                                                           */
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
 * @file IrLoader.cpp
 * @brief Implements the loading and conditioning of cabinet impulse responses.
 * @author Mustafa Garip
 */

#include "SushiDSP/io/IrLoader.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

#include "SushiDSP/io/WavFile.hpp"
#include "SushiDSP/math/Fft.hpp"
#include "SushiDSP/math/SincResampler.hpp"

namespace SushiDSP
{
    namespace
    {
        constexpr double PI = 3.14159265358979323846;

        std::uint32_t next_power_of_two(std::size_t value)
        {
            std::uint32_t size = 1;
            while (size < value)
            {
                size <<= 1;
            }
            return size;
        }

        void apply_fade_out(std::vector<float>& samples, std::size_t fade_length)
        {
            if (fade_length == 0 || samples.empty())
            {
                return;
            }

            fade_length = std::min(fade_length, samples.size());
            const std::size_t start = samples.size() - fade_length;
            for (std::size_t i = 0; i < fade_length; ++i)
            {
                const double gain = 0.5 * (1.0 + std::cos(PI * static_cast<double>(i) / static_cast<double>(fade_length)));
                samples[start + i] = static_cast<float>(samples[start + i] * gain);
            }
        }

        void normalise_band_average(std::vector<float>& samples, double sample_rate, float low_hz, float high_hz)
        {
            if (samples.empty())
            {
                return;
            }

            const std::uint32_t fft_size = next_power_of_two(samples.size() * 2);

            std::vector<float> padded(fft_size, 0.0f);
            std::copy(samples.begin(), samples.end(), padded.begin());

            std::vector<float> real_part(fft_size, 0.0f);
            std::vector<float> imaginary_part(fft_size, 0.0f);
            Fft(fft_size).forward(padded.data(), real_part.data(), imaginary_part.data());

            const double bin_width  = sample_rate / static_cast<double>(fft_size);
            const auto   first_bin  = static_cast<std::uint32_t>(std::ceil(low_hz / bin_width));
            const auto   last_bin   = std::min<std::uint32_t>(
                static_cast<std::uint32_t>(std::floor(high_hz / bin_width)), fft_size / 2);

            if (first_bin > last_bin)
            {
                return;
            }

            double decibel_sum = 0.0;
            std::uint32_t bin_count = 0;
            for (std::uint32_t bin = first_bin; bin <= last_bin; ++bin)
            {
                const double magnitude = std::sqrt(static_cast<double>(real_part[bin]) * real_part[bin]
                                                   + static_cast<double>(imaginary_part[bin]) * imaginary_part[bin]);
                decibel_sum += 20.0 * std::log10(std::max(magnitude, 1e-12));
                ++bin_count;
            }

            if (bin_count == 0)
            {
                return;
            }

            const double gain = std::pow(10.0, -(decibel_sum / bin_count) / 20.0);
            for (auto& sample : samples)
            {
                sample = static_cast<float>(sample * gain);
            }
        }
    }

    Ir IrLoader::load(const std::string& path, double target_sample_rate, const IrLoadOptions& options)
    {
        double source_rate = 0.0;
        auto   samples     = WavFile::load_mono(path, &source_rate);
        return prepare(std::move(samples), source_rate, target_sample_rate, options);
    }

    Ir IrLoader::prepare(std::vector<float> samples, double source_rate, double target_sample_rate,
                         const IrLoadOptions& options)
    {
        if (source_rate <= 0.0 || target_sample_rate <= 0.0)
        {
            throw std::invalid_argument("IrLoader::prepare: sample rates must be positive");
        }

        if (std::abs(source_rate - target_sample_rate) > 1e-6)
        {
            samples = SincResampler::resample(samples, source_rate, target_sample_rate);
        }

        const auto max_length = static_cast<std::size_t>(
            static_cast<double>(options.max_length_ms) * target_sample_rate / 1000.0);
        if (max_length > 0 && samples.size() > max_length)
        {
            samples.resize(max_length);
        }

        apply_fade_out(samples, static_cast<std::size_t>(
            static_cast<double>(options.fade_ms) * target_sample_rate / 1000.0));

        if (options.normalise)
        {
            normalise_band_average(samples, target_sample_rate, options.normalise_low_hz, options.normalise_high_hz);
        }

        return Ir{ std::move(samples), target_sample_rate };
    }
}
