/**************************************************************************/
/* test_tube_screamer_regression.cpp                                      */
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
// and <cmath> may otherwise be pulled in transitively before this point.
#define _USE_MATH_DEFINES

#include <cmath>
#include <vector>

#include <gtest/gtest.h>

#include "SushiDSP/tubescreamer/TubeScreamer.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

TEST(Regression_Dsp, TubeScreamerFixedInputProducesStableSnapshot)
{
    SushiDSP::TubeScreamer node;
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    node.prepare(info);
    node.set_drive(0.6f);
    node.set_tone(0.4f);
    node.set_level(0.8f);

    std::vector<float> input(256);
    for (std::size_t i = 0; i < input.size(); ++i)
    {
        input[i] = 0.25f * std::sin(2.0f * static_cast<float>(M_PI) * 440.0f * i / 48000.0f);
    }
    std::vector<float> output(256, 0.0f);
    const float* input_channels[1] = { input.data() };
    float* output_channels[1] = { output.data() };

    std::vector<float> last_block(256, 0.0f);
    for (int block = 0; block < 10; ++block)
    {
        SushiDSP::ProcessContext context{ output_channels, input_channels, 256, 0.0 };
        node.process(context);
        last_block = output;
    }

    // Snapshot: these five values are this implementation's own output at these indices of the
    // 10th block, captured once from the verified D3 implementation and pinned here to catch
    // future drift. They are not derived analytically — a change to any stage (input highpass,
    // clipper solve, oversampler, tone crossfade, level) will move them, which is the point.
    //
    // The 1e-5 tolerance is tight enough to catch a real behavioural change and loose enough to
    // absorb float reassociation within one toolchain. If a second compiler/platform is ever
    // added to CI and these drift by more than that, relax the tolerance rather than re-baking
    // the numbers — a re-bake would silently accept whatever the new code produces.
    // Re-captured after fixing kMinFeedbackResistance/kMaxFeedbackResistance from a
    // 1000x-too-small value.
    EXPECT_NEAR(last_block[0],   -0.0688199401f, 1e-5f);
    EXPECT_NEAR(last_block[64],   0.0454455949f, 1e-5f);
    EXPECT_NEAR(last_block[128],  0.0500506572f, 1e-5f);
    EXPECT_NEAR(last_block[192], -0.00388173456f, 1e-5f);
    EXPECT_NEAR(last_block[255], -0.0584274791f, 1e-5f);
}
