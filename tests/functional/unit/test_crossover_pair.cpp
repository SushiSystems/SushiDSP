/**************************************************************************/
/* test_crossover_pair.cpp                                                */
/**************************************************************************/
/*                         This file is part of:                          */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                        https://sushisystems.io                         */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License");        */
/* you may not use this file except in compliance with the License.       */
/* You may obtain a copy of the License at                                */
/*                                                                        */
/*     http://www.apache.org/licenses/LICENSE-2.0                         */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

/**
 * @file test_crossover_pair.cpp
 * @brief Tests for CrossoverPair, the power-complementary split between the wave and geometric layers.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "SushiDSP/math/Fft.hpp"
#include "SushiDSP/room/CrossoverPair.hpp"

namespace
{
    /// @brief Counts the points of the impulse response the spectrum is read from.
    constexpr std::uint32_t kFftSize = 8192;

    /// @brief Holds the sample rate every spectrum below is taken at.
    constexpr double kSampleRate = 48000.0;

    /// @brief Holds pi.
    constexpr double kPi = 3.14159265358979323846;

    /// @brief Holds the crossover the spec's example room puts at 278.2 Hz.
    constexpr double kCrossoverHz = 278.2;

    /// @brief Holds both halves' impulse responses and their sum.
    struct ImpulseResponses
    {
        /// @brief Stores the low half's response.
        std::vector<float> low;

        /// @brief Stores the high half's response.
        std::vector<float> high;

        /// @brief Stores the sample-by-sample sum of the two.
        std::vector<float> sum;
    };

    /// @brief Drives one unit impulse through a freshly configured pair.
    ImpulseResponses impulse_responses(double crossover_hz)
    {
        SushiDSP::room::CrossoverPair crossover;
        crossover.set_crossover(kSampleRate, crossover_hz);
        ImpulseResponses responses{ std::vector<float>(kFftSize), std::vector<float>(kFftSize),
                                    std::vector<float>(kFftSize) };
        for (std::uint32_t frame = 0; frame < kFftSize; ++frame)
        {
            float low = 0.0f;
            float high = 0.0f;
            crossover.process_sample(frame == 0 ? 1.0f : 0.0f, low, high);
            responses.low[frame] = low;
            responses.high[frame] = high;
            responses.sum[frame] = low + high;
        }
        return responses;
    }

    /// @brief Returns the magnitude in decibels of every bin of a real signal.
    std::vector<double> magnitude_db(const std::vector<float>& signal)
    {
        SushiDSP::Fft fft(kFftSize);
        std::vector<float> real(kFftSize);
        std::vector<float> imaginary(kFftSize);
        fft.forward(signal.data(), real.data(), imaginary.data());
        std::vector<double> result(kFftSize / 2 + 1);
        for (std::size_t bin = 0; bin < result.size(); ++bin)
        {
            const double power = static_cast<double>(real[bin]) * real[bin]
                               + static_cast<double>(imaginary[bin]) * imaginary[bin];
            result[bin] = 10.0 * std::log10(power + 1e-300);
        }
        return result;
    }

    /// @brief Returns the magnitude in decibels of a signal at one frequency, by direct DFT.
    double magnitude_db_at(const std::vector<float>& signal, double frequency_hz)
    {
        double real = 0.0;
        double imaginary = 0.0;
        const double omega = 2.0 * kPi * frequency_hz / kSampleRate;
        for (std::size_t frame = 0; frame < signal.size(); ++frame)
        {
            real += signal[frame] * std::cos(omega * static_cast<double>(frame));
            imaginary -= signal[frame] * std::sin(omega * static_cast<double>(frame));
        }
        return 10.0 * std::log10(real * real + imaginary * imaginary);
    }
}

TEST(Unit_Dsp, CrossoverPairSumsToUnityPowerEverywhere)
{
    // The halves feed two uncorrelated fields, so their powers add at the seam, and a
    // fourth-order Butterworth pair at one corner holds 1 / (1 + x^8) + x^8 / (1 + x^8) = 1
    // at every frequency; the bilinear transform keeps the identity exact in the digital
    // domain. What remains is float rounding and the response's tail past 8192 samples, both
    // under 0.001 dB, so the bound is 0.01 dB.
    const ImpulseResponses responses = impulse_responses(kCrossoverHz);
    const std::vector<double> low_db = magnitude_db(responses.low);
    const std::vector<double> high_db = magnitude_db(responses.high);
    const double bin_hz = kSampleRate / kFftSize;
    const auto first_bin = static_cast<std::size_t>(std::ceil(20.0 / bin_hz));
    const auto last_bin = static_cast<std::size_t>(std::floor(20000.0 / bin_hz));
    for (std::size_t bin = first_bin; bin <= last_bin; ++bin)
    {
        const double power = std::pow(10.0, low_db[bin] / 10.0) + std::pow(10.0, high_db[bin] / 10.0);
        EXPECT_NEAR(10.0 * std::log10(power), 0.0, 0.01)
            << "bin " << bin << " at " << bin * bin_hz << " Hz";
    }
}

TEST(Unit_Dsp, CrossoverPairIsMinusThreeDecibelsPerHalfAtTheCrossover)
{
    // Each half is a fourth-order Butterworth, at half power at its corner: -3.01 dB.
    const ImpulseResponses responses = impulse_responses(kCrossoverHz);
    EXPECT_NEAR(magnitude_db_at(responses.low, kCrossoverHz), -3.01, 0.05);
    EXPECT_NEAR(magnitude_db_at(responses.high, kCrossoverHz), -3.01, 0.05);
}

TEST(Unit_Dsp, CrossoverPairAmplitudeSumPeaksThreeDecibelsAtTheCrossover)
{
    // The price of power complementarity: the two halves are not phase-aligned, so one signal
    // sent through both and summed reads 2 / (0.7654 * 1.8478) = sqrt 2 at the corner,
    // +3.01 dB. No caller sums a correlated signal through the pair; this records the cost.
    const ImpulseResponses responses = impulse_responses(kCrossoverHz);
    EXPECT_NEAR(magnitude_db_at(responses.sum, kCrossoverHz), 3.01, 0.05);
}

TEST(Unit_Dsp, CrossoverPairFallsTwentyFourDecibelsPerOctave)
{
    // Two octaves above the crossover a fourth-order Butterworth sits 48 dB under the passband;
    // the closed form 10 log10(1 / (1 + 4^8)) is -48.16 dB.
    const ImpulseResponses responses = impulse_responses(kCrossoverHz);
    const double passband_db = magnitude_db_at(responses.low, 5.0);
    const double two_octaves_up_db = magnitude_db_at(responses.low, 4.0 * kCrossoverHz);
    EXPECT_NEAR(passband_db, 0.0, 0.01);
    EXPECT_NEAR(two_octaves_up_db - passband_db, -48.16, 0.1);
}

TEST(Unit_Dsp, CrossoverPairResetClearsItsState)
{
    SushiDSP::room::CrossoverPair crossover;
    crossover.set_crossover(kSampleRate, kCrossoverHz);
    float low = 0.0f;
    float high = 0.0f;
    crossover.process_sample(1.0f, low, high);
    crossover.reset();
    for (int frame = 0; frame < 64; ++frame)
    {
        crossover.process_sample(0.0f, low, high);
        EXPECT_LT(std::fabs(low), 1e-15f) << "frame " << frame;
        EXPECT_LT(std::fabs(high), 1e-15f) << "frame " << frame;
    }
}

TEST(Unit_Dsp, CrossoverPairAtZeroCrossoverPassesEverythingHigh)
{
    SushiDSP::room::CrossoverPair crossover;
    crossover.set_crossover(kSampleRate, 0.0);
    for (int frame = 0; frame < 256; ++frame)
    {
        const float input = static_cast<float>(std::sin(0.37 * frame));
        float low = 1.0f;
        float high = 0.0f;
        crossover.process_sample(input, low, high);
        EXPECT_EQ(low, 0.0f) << "frame " << frame;
        EXPECT_EQ(high, input) << "frame " << frame;
    }
}
