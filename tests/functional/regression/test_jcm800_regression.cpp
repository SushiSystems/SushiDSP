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

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "SushiDSP/jcm800/Jcm800.hpp"
#include "golden/GoldenFile.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{
    std::vector<float> render_jcm800(double sample_rate)
    {
        SushiDSP::Jcm800 node;
        // Knobs before prepare(): the smoothers snap, so the file holds the settled amp.
        node.set_preamp(0.6f);
        node.set_master(0.0f);
        node.set_bass(0.6f);
        node.set_middle(0.4f);
        node.set_treble(0.7f);
        node.set_presence(0.5f);
        node.prepare({ sample_rate, 256, 1 });
        const auto input = SushiDSP::golden::stimulus(sample_rate);
        std::vector<float> output(input.size(), 0.0f);
        for (std::size_t position = 0; position < input.size(); position += 256)
        {
            const std::uint32_t frames = static_cast<std::uint32_t>(std::min<std::size_t>(256, input.size() - position));
            const float* inputs[1] = { input.data() + position };
            float* outputs[1] = { output.data() + position };
            SushiDSP::ProcessContext context{ outputs, inputs, frames, 0.0 };
            node.process(context);
        }
        return output;
    }
}

// One second of the amp's output is pinned as tests/golden/jcm800_<rate>.f32 and compared by RMS
// residual. -80 dB catches any real behaviour change and absorbs float reassociation between
// toolchains. No cabinet is loaded so the file is independent of anything on disk.
//
// Regenerate deliberately, never to make a red test green:
//   SUSHIDSP_REGENERATE_GOLDEN=1 sd test -s regression
// and say why in the commit message and in tests/golden/README.md.
TEST(Regression_Dsp, Jcm800GoldenFile48k)
{
    EXPECT_LE(SushiDSP::golden::compare_or_regenerate("jcm800_48k", render_jcm800(48000.0)), -80.0);
}

TEST(Regression_Dsp, Jcm800GoldenFile96k)
{
    EXPECT_LE(SushiDSP::golden::compare_or_regenerate("jcm800_96k", render_jcm800(96000.0)), -80.0);
}
