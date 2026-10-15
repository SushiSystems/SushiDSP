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
 * @file test_jcm_tone_stack.cpp
 * @brief Tests the knobs and state of JcmToneStack.
 * @author Mustafa Garip
 */

// _USE_MATH_DEFINES precedes every include so that MSVC's <cmath> declares M_PI.
#define _USE_MATH_DEFINES

#include <cmath>

#include <gtest/gtest.h>

#include <SushiDSP/nodes/jcm800/jcm_tone_stack.hpp>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

TEST(Small_JcmToneStack, JcmToneStackAtFlatSettingsIsApproximatelyUnityGainAtMidFrequency)
{
    SushiDSP::JcmToneStack tone_stack(48000.0);
    tone_stack.set_bass(0.5f);
    tone_stack.set_middle(0.5f);
    tone_stack.set_treble(0.5f);

    constexpr double sample_rate = 48000.0;
    constexpr double test_freq = 1500.0;
    float max_output = 0.0f;
    for (int i = 0; i < 2000; ++i)
    {
        const float input = static_cast<float>(std::sin(2.0 * M_PI * test_freq * i / sample_rate));
        const float output = tone_stack.process_sample(input);
        if (i > 1000)
        {
            max_output = std::max(max_output, std::abs(output));
        }
    }

    EXPECT_NEAR(max_output, 1.0f, 0.15f);
}

TEST(Small_JcmToneStack, JcmToneStackBassKnobChangesLowFrequencyLevel)
{
    SushiDSP::JcmToneStack dark(48000.0);
    dark.set_bass(0.0f);
    dark.set_middle(0.5f);
    dark.set_treble(0.5f);
    SushiDSP::JcmToneStack bright(48000.0);
    bright.set_bass(1.0f);
    bright.set_middle(0.5f);
    bright.set_treble(0.5f);

    constexpr double sample_rate = 48000.0;
    constexpr double test_freq = 80.0;
    float dark_max = 0.0f, bright_max = 0.0f;
    for (int i = 0; i < 6000; ++i)
    {
        const float input = static_cast<float>(std::sin(2.0 * M_PI * test_freq * i / sample_rate));
        const float dark_out = dark.process_sample(input);
        const float bright_out = bright.process_sample(input);
        if (i > 5000)
        {
            dark_max = std::max(dark_max, std::abs(dark_out));
            bright_max = std::max(bright_max, std::abs(bright_out));
        }
    }

    EXPECT_GT(bright_max, dark_max * 1.5f);
}

TEST(Small_JcmToneStack, JcmToneStackMiddleKnobChangesMidFrequencyLevel)
{
    SushiDSP::JcmToneStack scooped(48000.0);
    scooped.set_bass(0.5f);
    scooped.set_middle(0.0f);
    scooped.set_treble(0.5f);
    SushiDSP::JcmToneStack boosted(48000.0);
    boosted.set_bass(0.5f);
    boosted.set_middle(1.0f);
    boosted.set_treble(0.5f);

    constexpr double sample_rate = 48000.0;
    constexpr double test_freq = 750.0;
    float scooped_max = 0.0f, boosted_max = 0.0f;
    for (int i = 0; i < 3000; ++i)
    {
        const float input = static_cast<float>(std::sin(2.0 * M_PI * test_freq * i / sample_rate));
        const float scooped_out = scooped.process_sample(input);
        const float boosted_out = boosted.process_sample(input);
        if (i > 2000)
        {
            scooped_max = std::max(scooped_max, std::abs(scooped_out));
            boosted_max = std::max(boosted_max, std::abs(boosted_out));
        }
    }

    EXPECT_GT(boosted_max, scooped_max * 1.5f);
}

TEST(Small_JcmToneStack, JcmToneStackResetClearsState)
{
    SushiDSP::JcmToneStack stack_a(48000.0);
    stack_a.set_bass(0.8f);
    stack_a.set_middle(0.2f);
    stack_a.set_treble(0.9f);
    SushiDSP::JcmToneStack stack_b(48000.0);
    stack_b.set_bass(0.8f);
    stack_b.set_middle(0.2f);
    stack_b.set_treble(0.9f);

    stack_a.process_sample(1.0f);
    stack_a.reset();

    const float from_reset = stack_a.process_sample(0.1f);
    const float from_fresh = stack_b.process_sample(0.1f);

    EXPECT_NEAR(from_reset, from_fresh, 1e-5f);
}

TEST(Small_JcmToneStack, JcmToneStackAtEightTimesRateMatchesBaseRateWithinFifthOfADecibel)
{
    // Float coefficient quantisation stays within 0.2 dB. See tests/nodes/jcm800/README.md.
    for (const double frequency : {80.0, 250.0, 1000.0, 4000.0, 10000.0})
    {
        double gain[2] = {0.0, 0.0};
        int index = 0;
        for (const double rate : {48000.0, 384000.0})
        {
            SushiDSP::JcmToneStack stack(rate);
            stack.set_bass(0.6f);
            stack.set_middle(0.4f);
            stack.set_treble(0.7f);
            const std::size_t length = static_cast<std::size_t>(rate);
            double input_power = 0.0, output_power = 0.0;
            for (std::size_t i = 0; i < length; ++i)
            {
                const float x = 0.1f * static_cast<float>(std::sin(2.0 * M_PI * frequency *
                                                                   static_cast<double>(i) / rate));
                const float y = stack.process_sample(x);
                if (i >= length / 2)
                {
                    input_power += static_cast<double>(x) * x;
                    output_power += static_cast<double>(y) * y;
                }
            }
            gain[index++] = 10.0 * std::log10(output_power / input_power);
        }
        EXPECT_NEAR(gain[0], gain[1], 0.2) << frequency << " Hz";
    }
}
