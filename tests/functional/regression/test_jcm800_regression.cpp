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

    // The knobs are set before prepare() deliberately. Every control is a smoothed target now, and
    // prepare() snaps the smoothers, so setting them first puts the amp in its settled
    // configuration from the first sample. What is snapshotted is then the amp's steady-state
    // response to this input - not a slice of whatever ramp the smoother happened to be on at
    // block 10, which is what the pre-D5b numbers captured and what made them hostage to the
    // smoothing law rather than to the circuit.
    node.set_preamp(0.6f);
    node.set_master(0.0f);
    node.set_bass(0.6f);
    node.set_middle(0.4f);
    node.set_treble(0.7f);
    node.set_presence(0.5f);
    node.prepare(info);

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
    // 10th block, pinned here to catch future drift.
    //
    // They were re-baked once, in D5b, and this is the reason. Before D5b the tone controls were
    // applied to the filters the instant the setter was called while preamp and master crawled
    // toward their targets by a fixed 10% of the remaining distance per block; the pinned numbers
    // therefore encoded where that block-rate ramp happened to be at block 10. D5b made every
    // control a uniformly smoothed target applied on the audio thread, which moves any output
    // captured mid-ramp - there is no version of this test that both keeps the old numbers and
    // exercises the new parameter path. Verified against a build of the pre-D5b implementation
    // that the rest of the chain is untouched: with no ramp in flight the two agree, and the
    // deltas here are the ramp alone.
    //
    // The setters now run before prepare() (see above) so what is pinned no longer depends on the
    // smoothing law at all: a future change to the time constant will not move these numbers,
    // which is what the earlier baseline could not promise. They are not derived analytically — a change to any stage (bright cap, V1a/
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
    EXPECT_NEAR(last_block[0],   -0.152692571f,  1e-5f);
    EXPECT_NEAR(last_block[64],  -0.0171861369f, 1e-5f);
    EXPECT_NEAR(last_block[128],  0.159566551f,  1e-5f);
    EXPECT_NEAR(last_block[192],  0.0254031960f, 1e-5f);
    EXPECT_NEAR(last_block[255], -0.154103413f,  1e-5f);
}
