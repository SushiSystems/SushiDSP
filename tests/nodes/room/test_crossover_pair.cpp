/**************************************************************************/
/* test_crossover_pair.cpp                                                */
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

#include <SushiDSP/foundation/allocation_trap/allocation_trap.hpp>
#include <SushiDSP/foundation/math/fft.hpp>
#include <SushiDSP/nodes/room/crossover_pair.hpp>

#include <support/tolerance.hpp>

namespace
{
    /// @brief Counts the points of the impulse response the spectrum is read from.
    constexpr std::uint32_t K_FFT_SIZE = 8192;

    /// @brief Holds the sample rate every spectrum below is taken at.
    constexpr double K_SAMPLE_RATE = 48000.0;

    /// @brief Holds pi.
    constexpr double K_PI = 3.14159265358979323846;

    /// @brief Holds the crossover the spec's example room puts at 278.2 Hz.
    constexpr double K_CROSSOVER_HZ = 278.2;

    /// @brief Characterised band, in decibels, of the power sum: float rounding and the response's
    /// tail past 8192 samples are both under 0.001 dB, so 0.01 dB. Kept from the original test.
    constexpr double K_POWER_SUM_BAND_DB = 0.01;

    /// @brief Characterised band, in decibels, of a level read by direct DFT of the 8192-point
    /// response at the corner. Kept from the original test.
    constexpr double K_CORNER_BAND_DB = 0.05;

    /// @brief Characterised band, in decibels, of the passband gain. Kept from the original test.
    constexpr double K_PASSBAND_BAND_DB = 0.01;

    /// @brief Characterised band, in decibels, of the level two octaves above the corner. Kept
    /// from the original test.
    constexpr double K_ROLLOFF_BAND_DB = 0.1;

    /// @brief Counts the frames of silence the reset and bypass checks run.
    constexpr int K_CHECK_FRAMES = 64;

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
        crossover.set_crossover(K_SAMPLE_RATE, crossover_hz);
        ImpulseResponses responses{std::vector<float>(K_FFT_SIZE), std::vector<float>(K_FFT_SIZE),
                                   std::vector<float>(K_FFT_SIZE)};
        for (std::uint32_t frame = 0; frame < K_FFT_SIZE; ++frame)
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
        SushiDSP::Fft fft(K_FFT_SIZE);
        std::vector<float> real(K_FFT_SIZE);
        std::vector<float> imaginary(K_FFT_SIZE);
        fft.forward(signal.data(), real.data(), imaginary.data());
        std::vector<double> result(K_FFT_SIZE / 2 + 1);
        for (std::size_t bin = 0; bin < result.size(); ++bin)
        {
            const double power = static_cast<double>(real[bin]) * real[bin] +
                                 static_cast<double>(imaginary[bin]) * imaginary[bin];
            result[bin] = 10.0 * std::log10(power + 1e-300);
        }
        return result;
    }

    /// @brief Returns the magnitude in decibels of a signal at one frequency, by direct DFT.
    double magnitude_db_at(const std::vector<float>& signal, double frequency_hz)
    {
        double real = 0.0;
        double imaginary = 0.0;
        const double omega = 2.0 * K_PI * frequency_hz / K_SAMPLE_RATE;
        for (std::size_t frame = 0; frame < signal.size(); ++frame)
        {
            real += signal[frame] * std::cos(omega * static_cast<double>(frame));
            imaginary -= signal[frame] * std::sin(omega * static_cast<double>(frame));
        }
        return 10.0 * std::log10(real * real + imaginary * imaginary);
    }

    /// @brief Expects @p computed to be @p reference within @p bound, in decibels.
    void expect_decibels_near(double computed, double reference, double bound)
    {
        EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(computed, reference, bound, 1.0)))
            << computed << " against " << reference;
    }

    /// @brief Expects the two halves' powers to sum to one, to within the band, at every bin from
    /// 20 Hz to 20 kHz.
    void expect_powers_sum_to_one(const ImpulseResponses& responses)
    {
        const std::vector<double> low_db = magnitude_db(responses.low);
        const std::vector<double> high_db = magnitude_db(responses.high);
        const double bin_hz = K_SAMPLE_RATE / K_FFT_SIZE;
        const auto first_bin = static_cast<std::size_t>(std::ceil(20.0 / bin_hz));
        const auto last_bin = static_cast<std::size_t>(std::floor(20000.0 / bin_hz));
        for (std::size_t bin = first_bin; bin <= last_bin; ++bin)
        {
            const double power = std::pow(10.0, low_db[bin] / 10.0) + std::pow(10.0, high_db[bin] / 10.0);
            expect_decibels_near(10.0 * std::log10(power), 0.0, K_POWER_SUM_BAND_DB);
        }
    }

    /// @brief Returns the closed-form level of the low half of a fourth-order Butterworth at
    /// @p frequency_hz when the corner is K_CROSSOVER_HZ, in decibels: the bilinear transform
    /// maps the analogue ratio f / fc to tan(pi f / fs) / tan(pi fc / fs).
    double butterworth_low_db(double frequency_hz)
    {
        const double ratio = std::tan(K_PI * frequency_hz / K_SAMPLE_RATE) /
                             std::tan(K_PI * K_CROSSOVER_HZ / K_SAMPLE_RATE);
        return -10.0 * std::log10(1.0 + std::pow(ratio, 8.0));
    }

    /// @brief Expects the pair, after a reset, to answer silence with exact silence.
    void expect_silent_after_reset(SushiDSP::room::CrossoverPair& crossover)
    {
        float low = 0.0f;
        float high = 0.0f;
        for (int frame = 0; frame < K_CHECK_FRAMES; ++frame)
        {
            crossover.process_sample(0.0f, low, high);
            EXPECT_EQ(low, 0.0f) << "frame " << frame;
            EXPECT_EQ(high, 0.0f) << "frame " << frame;
        }
    }

    /// @brief Expects a pair at zero crossover to answer zero low and the input as high.
    void expect_everything_high(SushiDSP::room::CrossoverPair& crossover)
    {
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

    /// @brief Fills a block with a 220 Hz tone at 0.3 of full scale.
    std::vector<float> room_tone()
    {
        std::vector<float> block(256);
        for (std::size_t i = 0; i < block.size(); ++i)
        {
            block[i] = 0.3f * std::sin(2.0f * static_cast<float>(K_PI) * 220.0f * static_cast<float>(i) / 48000.0f);
        }
        return block;
    }

    /// @brief Runs eight blocks through the pair, moving the crossover at blocks four and six, as
    /// the node does when it adopts a design.
    void process_blocks_moving_the_crossover(SushiDSP::room::CrossoverPair& pair, const std::vector<float>& input,
                                             std::vector<float>& low, std::vector<float>& high)
    {
        for (int block = 0; block < 8; ++block)
        {
            if (block == 4)
            {
                pair.set_crossover(48000.0, 356.1);
            }
            if (block == 6)
            {
                pair.set_crossover(48000.0, 0.0);
            }
            for (std::size_t i = 0; i < input.size(); ++i)
            {
                pair.process_sample(input[i], low[i], high[i]);
            }
        }
    }

    /// @brief Expects every sample of @p samples to be finite.
    void expect_all_finite(const std::vector<float>& samples)
    {
        for (const float sample : samples)
        {
            EXPECT_TRUE(std::isfinite(sample));
        }
    }
} // namespace

TEST(Small_CrossoverPair, CrossoverPairSumsToUnityPowerEverywhere)
{
    // The pair's powers sum to one at every frequency. See tests/nodes/room/README.md.
    expect_powers_sum_to_one(impulse_responses(K_CROSSOVER_HZ));
}

TEST(Small_CrossoverPair, CrossoverPairLowHalfIsMinusThreeDecibelsAtTheCrossover)
{
    // Each half is a fourth-order Butterworth, at half power at its corner: -3.01 dB.
    const ImpulseResponses responses = impulse_responses(K_CROSSOVER_HZ);

    expect_decibels_near(magnitude_db_at(responses.low, K_CROSSOVER_HZ), 10.0 * std::log10(0.5), K_CORNER_BAND_DB);
}

TEST(Small_CrossoverPair, CrossoverPairHighHalfIsMinusThreeDecibelsAtTheCrossover)
{
    const ImpulseResponses responses = impulse_responses(K_CROSSOVER_HZ);

    expect_decibels_near(magnitude_db_at(responses.high, K_CROSSOVER_HZ), 10.0 * std::log10(0.5), K_CORNER_BAND_DB);
}

TEST(Small_CrossoverPair, CrossoverPairAmplitudeSumPeaksThreeDecibelsAtTheCrossover)
{
    // The amplitude sum reads +3.01 dB at the corner. See tests/nodes/room/README.md.
    const ImpulseResponses responses = impulse_responses(K_CROSSOVER_HZ);

    expect_decibels_near(magnitude_db_at(responses.sum, K_CROSSOVER_HZ), 20.0 * std::log10(std::sqrt(2.0)),
                         K_CORNER_BAND_DB);
}

TEST(Small_CrossoverPair, CrossoverPairLowHalfHasUnityGainInThePassband)
{
    const ImpulseResponses responses = impulse_responses(K_CROSSOVER_HZ);

    expect_decibels_near(magnitude_db_at(responses.low, 5.0), 0.0, K_PASSBAND_BAND_DB);
}

TEST(Small_CrossoverPair, CrossoverPairFallsTwentyFourDecibelsPerOctave)
{
    // The closed form two octaves up is -48.16 dB. See tests/nodes/room/README.md.
    const ImpulseResponses responses = impulse_responses(K_CROSSOVER_HZ);

    expect_decibels_near(magnitude_db_at(responses.low, 4.0 * K_CROSSOVER_HZ),
                         butterworth_low_db(4.0 * K_CROSSOVER_HZ), K_ROLLOFF_BAND_DB);
}

TEST(Small_CrossoverPair, CrossoverPairResetClearsItsState)
{
    SushiDSP::room::CrossoverPair crossover;
    crossover.set_crossover(K_SAMPLE_RATE, K_CROSSOVER_HZ);
    float low = 0.0f;
    float high = 0.0f;
    crossover.process_sample(1.0f, low, high);
    crossover.reset();

    expect_silent_after_reset(crossover);
}

TEST(Small_CrossoverPair, CrossoverPairAtZeroCrossoverPassesEverythingHigh)
{
    SushiDSP::room::CrossoverPair crossover;
    crossover.set_crossover(K_SAMPLE_RATE, 0.0);

    expect_everything_high(crossover);
}

TEST(Medium_CrossoverPair, ProcessPerformsNoHeapAllocation)
{
    // set_crossover is arithmetic on the audio thread. See tests/nodes/room/README.md.
    SushiDSP::room::CrossoverPair pair;
    const std::vector<float> input = room_tone();
    std::vector<float> low(256, 0.0f);
    std::vector<float> high(256, 0.0f);

    SushiDSP::test::AllocationTrap trap;
    trap.arm();
    pair.set_crossover(48000.0, 278.18);
    process_blocks_moving_the_crossover(pair, input, low, high);
    pair.reset();
    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
    expect_all_finite(low);
    expect_all_finite(high);
}
