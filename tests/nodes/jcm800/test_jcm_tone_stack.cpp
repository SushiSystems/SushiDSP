/**************************************************************************/
/* test_jcm_tone_stack.cpp                                                */
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
 * @file test_jcm_tone_stack.cpp
 * @brief Tests the JCM800 tone stack's flat gain, its bass and middle knobs, its reset and its rate invariance.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <SushiDSP/nodes/jcm800/jcm_tone_stack.hpp>
#include <SushiDSP/foundation/signal_measure/render.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace
{
    constexpr double K_SAMPLE_RATE = 48000.0;

    /// @brief How far the flat stack's midband gain may sit from unity. Characterized: the stack
    /// at noon is "approximately" unity at 1.5 kHz, and the reading is within 15 %.
    constexpr double K_FLAT_UNITY_BAND = 0.15;

    /// @brief The least ratio by which the knob's high end must exceed its low end in level.
    /// Characterized: the knobs move their band by well over this.
    constexpr double K_KNOB_LIFT_RATIO = 1.5;

    /// @brief How far the gain at 8 times the rate may sit from the gain at the base rate, in dB.
    /// Characterized: the float coefficients of the 384 kHz design quantise within 0.2 dB.
    constexpr double K_RATE_AGREEMENT_DB = 0.2;

    /// @brief Seconds the rate test runs; the last half is measured, and every probe frequency
    /// fits a whole number of cycles in it.
    constexpr double K_RATE_RUN_SECONDS = 0.4;

    /// @brief The largest magnitude of a sine through a stack from sample `settled_from` on.
    float settled_peak(SushiDSP::JcmToneStack& stack, double frequency_hz, std::size_t samples,
                       std::size_t settled_from, float amplitude)
    {
        const std::vector<float> input = SushiDSP::measure::sine_stimulus(frequency_hz, K_SAMPLE_RATE, samples, amplitude);
        float peak = 0.0f;
        for (std::size_t i = 0; i < input.size(); ++i)
        {
            const float output = stack.process_sample(input[i]);
            if (i > settled_from)
            {
                peak = std::max(peak, std::fabs(output));
            }
        }
        return peak;
    }

    /// @brief A stack with the three knobs set.
    SushiDSP::JcmToneStack stack_with(double sample_rate, float bass, float middle, float treble)
    {
        SushiDSP::JcmToneStack stack(sample_rate);
        stack.set_bass(bass);
        stack.set_middle(middle);
        stack.set_treble(treble);
        return stack;
    }

    /// @brief The power gain, in dB, of a sine through the stack over the last half of its run.
    double power_gain_db(double frequency_hz, double sample_rate)
    {
        SushiDSP::JcmToneStack stack = stack_with(sample_rate, 0.6f, 0.4f, 0.7f);
        const std::size_t length = static_cast<std::size_t>(K_RATE_RUN_SECONDS * sample_rate);
        const std::vector<float> input = SushiDSP::measure::sine_stimulus(frequency_hz, sample_rate, length, 0.1f);
        double input_power = 0.0;
        double output_power = 0.0;
        for (std::size_t i = 0; i < length; ++i)
        {
            const float y = stack.process_sample(input[i]);
            if (i >= length / 2)
            {
                input_power += static_cast<double>(input[i]) * input[i];
                output_power += static_cast<double>(y) * y;
            }
        }
        return 10.0 * std::log10(output_power / input_power);
    }
} // namespace

TEST(Small_JcmToneStack, JcmToneStackAtFlatSettingsIsApproximatelyUnityGainAtMidFrequency)
{
    SushiDSP::JcmToneStack stack = stack_with(K_SAMPLE_RATE, 0.5f, 0.5f, 0.5f);
    EXPECT_NEAR(settled_peak(stack, 1500.0, 2000u, 1000u, 1.0f), 1.0, K_FLAT_UNITY_BAND);
}

TEST(Small_JcmToneStack, JcmToneStackBassKnobChangesLowFrequencyLevel)
{
    SushiDSP::JcmToneStack dark = stack_with(K_SAMPLE_RATE, 0.0f, 0.5f, 0.5f);
    SushiDSP::JcmToneStack bright = stack_with(K_SAMPLE_RATE, 1.0f, 0.5f, 0.5f);
    const float dark_peak = settled_peak(dark, 80.0, 6000u, 5000u, 1.0f);
    const float bright_peak = settled_peak(bright, 80.0, 6000u, 5000u, 1.0f);
    EXPECT_GT(bright_peak, dark_peak * K_KNOB_LIFT_RATIO);
}

TEST(Small_JcmToneStack, JcmToneStackMiddleKnobChangesMidFrequencyLevel)
{
    SushiDSP::JcmToneStack scooped = stack_with(K_SAMPLE_RATE, 0.5f, 0.0f, 0.5f);
    SushiDSP::JcmToneStack boosted = stack_with(K_SAMPLE_RATE, 0.5f, 1.0f, 0.5f);
    const float scooped_peak = settled_peak(scooped, 750.0, 3000u, 2000u, 1.0f);
    const float boosted_peak = settled_peak(boosted, 750.0, 3000u, 2000u, 1.0f);
    EXPECT_GT(boosted_peak, scooped_peak * K_KNOB_LIFT_RATIO);
}

TEST(Small_JcmToneStack, JcmToneStackResetClearsState)
{
    SushiDSP::JcmToneStack used = stack_with(K_SAMPLE_RATE, 0.8f, 0.2f, 0.9f);
    SushiDSP::JcmToneStack fresh = stack_with(K_SAMPLE_RATE, 0.8f, 0.2f, 0.9f);

    used.process_sample(1.0f);
    used.reset();

    EXPECT_EQ(used.process_sample(0.1f), fresh.process_sample(0.1f));
}

TEST(Small_JcmToneStack, JcmToneStackAtEightTimesRateMatchesBaseRateWithinFifthOfADecibel)
{
    // Float coefficient quantisation stays within 0.2 dB. See tests/nodes/jcm800/README.md.
    for (const double frequency : {80.0, 250.0, 1000.0, 4000.0, 10000.0})
    {
        EXPECT_NEAR(power_gain_db(frequency, 48000.0), power_gain_db(frequency, 384000.0), K_RATE_AGREEMENT_DB)
            << frequency << " Hz";
    }
}
