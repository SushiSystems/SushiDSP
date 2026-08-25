/**************************************************************************/
/* test_jcm_tone_stack.cpp                                                */
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

// _USE_MATH_DEFINES must precede every include: it is what makes MSVC's <cmath> declare M_PI,
// and it must remain the very first thing in the translation unit.
#define _USE_MATH_DEFINES

#include <cmath>

#include <gtest/gtest.h>

#include "SushiDSP/jcm800/JcmToneStack.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

TEST(Unit_Dsp, JcmToneStackAtFlatSettingsIsApproximatelyUnityGainAtMidFrequency)
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

TEST(Unit_Dsp, JcmToneStackBassKnobChangesLowFrequencyLevel)
{
    SushiDSP::JcmToneStack dark(48000.0);
    dark.set_bass(0.0f); dark.set_middle(0.5f); dark.set_treble(0.5f);
    SushiDSP::JcmToneStack bright(48000.0);
    bright.set_bass(1.0f); bright.set_middle(0.5f); bright.set_treble(0.5f);

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

TEST(Unit_Dsp, JcmToneStackMiddleKnobChangesMidFrequencyLevel)
{
    SushiDSP::JcmToneStack scooped(48000.0);
    scooped.set_bass(0.5f); scooped.set_middle(0.0f); scooped.set_treble(0.5f);
    SushiDSP::JcmToneStack boosted(48000.0);
    boosted.set_bass(0.5f); boosted.set_middle(1.0f); boosted.set_treble(0.5f);

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

TEST(Unit_Dsp, JcmToneStackResetClearsState)
{
    SushiDSP::JcmToneStack stack_a(48000.0);
    stack_a.set_bass(0.8f); stack_a.set_middle(0.2f); stack_a.set_treble(0.9f);
    SushiDSP::JcmToneStack stack_b(48000.0);
    stack_b.set_bass(0.8f); stack_b.set_middle(0.2f); stack_b.set_treble(0.9f);

    stack_a.process_sample(1.0f);
    stack_a.reset();

    const float from_reset = stack_a.process_sample(0.1f);
    const float from_fresh = stack_b.process_sample(0.1f);

    EXPECT_NEAR(from_reset, from_fresh, 1e-5f);
}
