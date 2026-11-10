/**************************************************************************/
/* test_dd3_converter.cpp                                                 */
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
 * @file test_dd3_converter.cpp
 * @brief Tests for the DD-3 12-bit converter model.
 * @author Mustafa Garip & Sushi Systems
 */

// _USE_MATH_DEFINES precedes every include so that MSVC's <cmath> declares M_PI.
#define _USE_MATH_DEFINES

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include <SushiDSP/nodes/dd3/dd3_converter.hpp>

#include "support/tolerance.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{
    using SushiDSP::dd3::Dd3Converter;
    using SushiDSP::dd3::Dd3Mode;

    /// @brief The index a search returns when no output was non-zero.
    constexpr std::size_t K_NONE = std::numeric_limits<std::size_t>::max();

    /// @brief The sine of the noise test: 997 Hz at 48 kHz, which shares no factor with 48000, so one
    /// second visits 48000 distinct, equally spaced phases.
    constexpr double K_NOISE_TONE_HZ = 997.0;
    constexpr double K_NOISE_SAMPLE_RATE = 48000.0;
    constexpr std::size_t K_NOISE_PHASES = 48000;

    /// @brief How many standard deviations of the sampled error power the SNR may sit from the ideal.
    constexpr double K_NOISE_SIGMAS = 6.0;

    /// @brief The sample at which the Hold test writes its impulse, in a ring of K_LONG_LENGTH.
    constexpr std::uint32_t K_HOLD_IMPULSE_AT = 100u;

    /// @brief The code the converter gives a 1 V impulse: 1/step = 819.2 rounds to 819.
    constexpr double K_UNIT_IMPULSE_CODE = 819.0;

    /// @brief Names an input and the code the converter must answer with.
    struct SaturationRow
    {
        const char* name;
        double volts;
        double code;
    };

    const SaturationRow K_SATURATION_ROWS[] = {{"PositiveTen", 10.0, 2047.0},
                                               {"PositiveMillion", 1e6, 2047.0},
                                               {"NegativeTen", -10.0, -2048.0}};

    /// @brief Names a delay mode and its ring length.
    struct ModeRow
    {
        const char* name;
        Dd3Mode mode;
        std::uint32_t length;
    };

    const ModeRow K_MODE_ROWS[] = {{"Short", Dd3Mode::Short, 1024u},
                                   {"Middle", Dd3Mode::Middle, 4096u},
                                   {"Long", Dd3Mode::Long, 16384u}};

    /// @brief A converter prepared and set to a mode.
    Dd3Converter prepared_converter(Dd3Mode mode)
    {
        Dd3Converter converter;
        converter.prepare();
        converter.set_mode(mode);
        return converter;
    }

    /// @brief Feeds `count` samples of `value` and returns what the converter answered.
    std::vector<float> feed_constant(Dd3Converter& converter, float value, std::uint32_t count)
    {
        std::vector<float> answers;
        answers.reserve(count);
        for (std::uint32_t n = 0; n < count; ++n)
        {
            answers.push_back(converter.process_sample(value));
        }
        return answers;
    }

    /// @brief Feeds a unit impulse at sample 0 followed by zeros and returns the index of the first
    /// non-zero answer within `count` samples, or K_NONE.
    std::size_t first_answer_to_an_impulse(Dd3Converter& converter, std::uint32_t count)
    {
        for (std::uint32_t n = 0; n < count; ++n)
        {
            if (converter.process_sample(n == 0 ? 1.0f : 0.0f) != 0.0f)
            {
                return n;
            }
        }
        return K_NONE;
    }

    /// @brief Returns the index of the first non-zero entry, or K_NONE.
    std::size_t first_non_zero(const std::vector<float>& answers)
    {
        for (std::size_t n = 0; n < answers.size(); ++n)
        {
            if (answers[n] != 0.0f)
            {
                return n;
            }
        }
        return K_NONE;
    }

    /// @brief Writes an impulse of 1 V at K_HOLD_IMPULSE_AT into a Long ring, one lap.
    void store_the_hold_impulse(Dd3Converter& converter)
    {
        for (std::uint32_t n = 0; n < Dd3Converter::K_LONG_LENGTH; ++n)
        {
            converter.process_sample(n == K_HOLD_IMPULSE_AT ? 1.0f : 0.0f);
        }
    }

    /// @brief Expects a lap of answers to be the stored impulse and zeros.
    void expect_the_stored_impulse(const std::vector<float>& lap, std::uint32_t lap_index)
    {
        for (std::size_t n = 0; n < lap.size(); ++n)
        {
            const float expected =
                n == K_HOLD_IMPULSE_AT ? static_cast<float>(K_UNIT_IMPULSE_CODE * Dd3Converter::K_STEP_VOLTS) : 0.0f;
            EXPECT_EQ(lap[n], expected) << "lap " << lap_index << " sample " << n;
        }
    }

    /// @brief The error power and signal power of a full-scale sine through the quantiser, with the
    /// sine clamped to the top code as the converter clamps it.
    struct NoiseAudit
    {
        double signal_power;
        double error_power;
    };

    /// @brief Quantises one second of the noise tone and sums the powers.
    NoiseAudit audit_quantisation_noise()
    {
        NoiseAudit audit{0.0, 0.0};
        for (std::size_t n = 0; n < K_NOISE_PHASES; ++n)
        {
            const double input = Dd3Converter::K_FULL_SCALE_VOLTS *
                                 std::sin(2.0 * M_PI * K_NOISE_TONE_HZ * static_cast<double>(n) / K_NOISE_SAMPLE_RATE);
            const double clamped = std::fmin(input, Dd3Converter::K_MAXIMUM_CODE * Dd3Converter::K_STEP_VOLTS);
            const double output = Dd3Converter::quantise(clamped);
            audit.signal_power += clamped * clamped;
            audit.error_power += (output - clamped) * (output - clamped);
        }
        return audit;
    }
} // namespace

TEST(Small_Dd3Converter, Dd3ConverterQuantisesOnItsExactCodeBoundaries)
{
    // step = 2 * 2.5 / 4096 = 5/4096 = 1.220703125e-3 V exactly  (DR1+DR2 plan, N8).
    using Converter = SushiDSP::dd3::Dd3Converter;
    EXPECT_DOUBLE_EQ(Converter::K_STEP_VOLTS, 1.220703125e-3);
    EXPECT_DOUBLE_EQ(Converter::quantise(0.0), 0.0);
    EXPECT_DOUBLE_EQ(Converter::quantise(0.49 * Converter::K_STEP_VOLTS), 0.0);
    EXPECT_DOUBLE_EQ(Converter::quantise(0.51 * Converter::K_STEP_VOLTS), Converter::K_STEP_VOLTS);
    EXPECT_DOUBLE_EQ(Converter::quantise(-0.51 * Converter::K_STEP_VOLTS),
                     -Converter::K_STEP_VOLTS);
    EXPECT_DOUBLE_EQ(Converter::quantise(1.0 * Converter::K_STEP_VOLTS), Converter::K_STEP_VOLTS);
    // The exact tie rounds away from zero, not to even.
    EXPECT_DOUBLE_EQ(Converter::quantise(0.5 * Converter::K_STEP_VOLTS), Converter::K_STEP_VOLTS);
    EXPECT_DOUBLE_EQ(Converter::quantise(-0.5 * Converter::K_STEP_VOLTS), -Converter::K_STEP_VOLTS);
}

TEST(Small_Dd3Converter, Dd3ConverterSaturatesRatherThanWrapping)
{
    // Ruling 15: code 2047 is the top and code -2048 the bottom, and code * step is exact.
    for (const SaturationRow& row : K_SATURATION_ROWS)
    {
        EXPECT_EQ(Dd3Converter::quantise(row.volts), row.code * Dd3Converter::K_STEP_VOLTS) << row.name;
    }
}

TEST(Small_Dd3Converter, Dd3ConverterQuantisationNoiseMatchesTwelveBits)
{
    // An ideal 12-bit converter gives 74.008 dB of SNR. See tests/nodes/dd3/README.md.
    const NoiseAudit audit = audit_quantisation_noise();
    const double measured_db = 10.0 * std::log10(audit.signal_power / audit.error_power);
    const double ideal_db = 10.0 * std::log10(6.0 * 2048.0 * 2048.0);
    const double relative_sigma = std::sqrt(12.0 * 12.0 / 180.0 / static_cast<double>(K_NOISE_PHASES));
    const double bound_db = 10.0 * std::log10(1.0 + K_NOISE_SIGMAS * relative_sigma);

    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(measured_db, ideal_db, bound_db, 1.0)));
}

TEST(Small_Dd3Converter, Dd3ConverterRingDelaysByExactlyNSamples)
{
    for (const ModeRow& row : K_MODE_ROWS)
    {
        Dd3Converter converter = prepared_converter(row.mode);

        EXPECT_EQ(converter.ring_length(), row.length) << row.name;
        EXPECT_EQ(first_answer_to_an_impulse(converter, 3u * row.length), row.length) << row.name;
    }
}

TEST(Small_Dd3Converter, Dd3ConverterHoldKeepsTheLoopAndInhibitsWrites)
{
    // Long and Hold share N, so the ring is not cleared. See tests/nodes/dd3/README.md.
    Dd3Converter converter = prepared_converter(Dd3Mode::Long);
    store_the_hold_impulse(converter);
    converter.set_mode(Dd3Mode::Hold);

    for (std::uint32_t lap = 0; lap < 3u; ++lap)
    {
        expect_the_stored_impulse(feed_constant(converter, 0.5f, Dd3Converter::K_LONG_LENGTH), lap);
    }
    EXPECT_EQ(converter.ring_length(), Dd3Converter::K_LONG_LENGTH);
}

TEST(Small_Dd3Converter, Dd3ConverterModeChangeClearsTheRingWhenNChanges)
{
    // Spec: "MODE changes swap N and clear the ring" - which is exactly the S/M/L case.
    Dd3Converter converter = prepared_converter(Dd3Mode::Short);
    feed_constant(converter, 0.9f, 512u);
    converter.set_mode(Dd3Mode::Middle);

    EXPECT_EQ(converter.ring_length(), Dd3Converter::K_MIDDLE_LENGTH);
    EXPECT_EQ(first_non_zero(feed_constant(converter, 0.0f, Dd3Converter::K_MIDDLE_LENGTH)), K_NONE);
}
