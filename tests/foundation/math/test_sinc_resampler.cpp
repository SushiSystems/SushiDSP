/**************************************************************************/
/* test_sinc_resampler.cpp                                                */
/**************************************************************************/
/*                          This file is part of:                         */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                         https://sushisystems.io                        */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License"); you    */
/* may not use this file except in compliance with the License. You may   */
/* obtain a copy of the License at                                        */
/*                                                                        */
/*     https://www.apache.org/licenses/LICENSE-2.0                        */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

/**
 * @file test_sinc_resampler.cpp
 * @brief Tests that SincResampler preserves a tone and low-passes when downsampling.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <vector>

#include <SushiDSP/foundation/math/sinc_resampler.hpp>

namespace
{
    /// Pi to double precision, spelled out so the tests do not depend on M_PI being visible.
    constexpr double K_PI = 3.14159265358979323846;

    /// The level of energy relative to the tone below which a preserved tone's error must fall, in
    /// decibels: a characterisation level, 6.7 dB inside the 86.7 dB the Kaiser window (beta 8.6) gives.
    constexpr double K_PRESERVATION_LEVEL_DB = -80.0;

    /// The level of a stopband tone's energy relative to a passband tone below which the output must fall,
    /// in decibels: a characterisation level, 26.7 dB inside the same 86.7 dB.
    constexpr double K_REJECTION_LEVEL_DB = -60.0;

    /// The samples skipped at each end of an output, where the kernel runs off the signal.
    constexpr std::size_t K_EDGE = 1000;

    /// @brief Returns a unit sine of the given frequency sampled at the given rate.
    std::vector<float> sine(double frequency_hz, double sample_rate, std::size_t length)
    {
        std::vector<float> signal(length);
        for (std::size_t i = 0; i < length; ++i)
        {
            signal[i] = static_cast<float>(std::sin(2.0 * K_PI * frequency_hz * static_cast<double>(i) / sample_rate));
        }
        return signal;
    }

    /// @brief Returns the energy of the error against the analytic sine, relative to that sine, in decibels.
    double error_energy_db(const std::vector<float>& output, double frequency_hz, double sample_rate)
    {
        double error = 0.0;
        double energy = 0.0;
        for (std::size_t i = K_EDGE; i + K_EDGE < output.size(); ++i)
        {
            const double expected = std::sin(2.0 * K_PI * frequency_hz * static_cast<double>(i) / sample_rate);
            error += (output[i] - expected) * (output[i] - expected);
            energy += expected * expected;
        }
        return 10.0 * std::log10(error / energy);
    }

    /// @brief Returns the mean energy of a tone's output after the rate is halved.
    double mean_energy_after_halving_the_rate(double frequency_hz)
    {
        const std::vector<float> output =
            SushiDSP::SincResampler::resample(sine(frequency_hz, 96000.0, 96000), 96000.0, 48000.0);
        double energy = 0.0;
        for (std::size_t i = K_EDGE; i + K_EDGE < output.size(); ++i)
        {
            energy += static_cast<double>(output[i]) * output[i];
        }
        return energy / static_cast<double>(output.size() - 2 * K_EDGE);
    }
} // namespace

TEST(Medium_SincResampler, OutputLengthIsTheRatioRoundedToTheNearestSample)
{
    const std::vector<float> output = SushiDSP::SincResampler::resample(sine(1000.0, 44100.0, 44100), 44100.0, 48000.0);

    EXPECT_EQ(output.size(), 48000u);
}

TEST(Medium_SincResampler, PreservesAOneKilohertzToneBelowThePreservationLevel)
{
    const std::vector<float> output = SushiDSP::SincResampler::resample(sine(1000.0, 44100.0, 44100), 44100.0, 48000.0);

    EXPECT_LT(error_energy_db(output, 1000.0, 48000.0), K_PRESERVATION_LEVEL_DB);
}

TEST(Medium_SincResampler, LowpassesAboveTheNewNyquistWhenDownsampling)
{
    const double passband_energy = mean_energy_after_halving_the_rate(1000.0);
    const double aliased_energy = mean_energy_after_halving_the_rate(30000.0);

    EXPECT_LT(10.0 * std::log10(aliased_energy / passband_energy), K_REJECTION_LEVEL_DB);
}
