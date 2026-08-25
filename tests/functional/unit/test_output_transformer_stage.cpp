/**************************************************************************/
/* test_output_transformer_stage.cpp                                      */
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
// which GCC/Clang expose unconditionally.
#define _USE_MATH_DEFINES
#include <cmath>

#include <gtest/gtest.h>

#include "SushiDSP/jcm800/OutputTransformerStage.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

TEST(Unit_Dsp, OutputTransformerStageBlocksDc)
{
    SushiDSP::OutputTransformerStage stage(48000.0, 60.0f, 8000.0f, 0.3f);

    float output = 0.0f;
    for (int i = 0; i < 10000; ++i)
    {
        output = stage.process_sample(1.0f);
    }

    EXPECT_NEAR(output, 0.0f, 1e-2f);
}

TEST(Unit_Dsp, OutputTransformerStageSaturatesLargeSignalsToFiniteOutput)
{
    SushiDSP::OutputTransformerStage stage(48000.0, 60.0f, 8000.0f, 0.3f);

    constexpr double sample_rate = 48000.0;
    constexpr double test_freq = 200.0;
    float max_output = 0.0f;
    for (int i = 0; i < 4000; ++i)
    {
        const float input = 50.0f * static_cast<float>(std::sin(2.0 * M_PI * test_freq * i / sample_rate));
        const float output = stage.process_sample(input);
        ASSERT_TRUE(std::isfinite(output));
        if (i > 3000)
        {
            max_output = std::max(max_output, std::abs(output));
        }
    }

    EXPECT_LT(max_output, 1.0f / 0.3f + 1.0f);
}

TEST(Unit_Dsp, OutputTransformerStageResetClearsFilterState)
{
    SushiDSP::OutputTransformerStage stage_a(48000.0, 60.0f, 8000.0f, 0.3f);
    SushiDSP::OutputTransformerStage stage_b(48000.0, 60.0f, 8000.0f, 0.3f);

    stage_a.process_sample(1.0f);
    stage_a.reset();

    const float from_reset = stage_a.process_sample(0.2f);
    const float from_fresh = stage_b.process_sample(0.2f);

    EXPECT_NEAR(from_reset, from_fresh, 1e-5f);
}
