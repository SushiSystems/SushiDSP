/**************************************************************************/
/* test_jcm800_regression.cpp                                             */
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

#include "SushiDSP/jcm800/Jcm800.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

TEST(Regression_Dsp, Jcm800FixedInputProducesStableSnapshot)
{
    SushiDSP::Jcm800 node;
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    node.prepare(info);
    node.set_preamp(0.6f);
    node.set_master(0.7f);
    node.set_bass(0.6f);
    node.set_middle(0.4f);
    node.set_treble(0.7f);
    node.set_presence(0.5f);

    std::vector<float> input(256);
    for (std::size_t i = 0; i < input.size(); ++i)
    {
        input[i] = 0.25f * std::sin(2.0f * static_cast<float>(M_PI) * 220.0f * i / 48000.0f);
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
    // 10th block, captured once from the verified D4 implementation and pinned here to catch
    // future drift. They are not derived analytically — a change to any stage (bright cap, V1a/
    // V1b, coupling cap, tone stack, V2a, phase inverter, EL34 pair, output transformer, sag,
    // presence, master) will move them, which is the point.
    //
    // No cabinet IR is loaded, deliberately: it keeps the snapshot independent of any file on
    // disk, and it also keeps the pinned numbers independent of PartitionedConvolution's lack of
    // a reset(), which would otherwise leave convolution tail state outside this test's control.
    //
    // The 1e-5 tolerance is tight enough to catch a real behavioural change and loose enough to
    // absorb float reassociation within one toolchain. If a second compiler/platform is ever
    // added to CI and these drift by more than that, relax the tolerance rather than re-baking
    // the numbers — a re-bake would silently accept whatever the new code produces.
    EXPECT_NEAR(last_block[0],   -0.14973262f,   1e-5f);
    EXPECT_NEAR(last_block[64],  -0.012345654f,  1e-5f);
    EXPECT_NEAR(last_block[128],  0.152603552f,  1e-5f);
    EXPECT_NEAR(last_block[192],  0.0228477269f, 1e-5f);
    EXPECT_NEAR(last_block[255], -0.15123713f,   1e-5f);
}
