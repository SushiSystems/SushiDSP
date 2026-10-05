/****************************************************************/
/* test_tube_screamer_regression.cpp                            */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

// _USE_MATH_DEFINES must precede every include: it is what makes MSVC's <cmath> declare M_PI,
// and <cmath> may otherwise be pulled in transitively before this point.
#define _USE_MATH_DEFINES

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "SushiDSP/tubescreamer/TubeScreamer.hpp"
#include "golden/GoldenFile.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{
    std::vector<float> render_tube_screamer(double sample_rate)
    {
        SushiDSP::TubeScreamer node;
        // Knobs before prepare(): the smoothers snap, so the file holds the settled pedal.
        node.set_drive(0.6f);
        node.set_tone(0.4f);
        node.set_level(SushiDSP::gain_to_decibels(0.8f / SushiDSP::TubeScreamer::kLevelUnityGain));
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

// One second of the pedal's output is pinned as tests/golden/tube_screamer_<rate>.f32 and
// compared by RMS residual. -80 dB catches any real behaviour change and absorbs float
// reassociation between toolchains.
//
// Regenerate deliberately, never to make a red test green:
//   SUSHIDSP_REGENERATE_GOLDEN=1 sd test -s regression
// and say why in the commit message and in tests/golden/README.md.
// Expected red until Task 8 re-pins against the DK chain at 4x - see the D6c plan.
TEST(Regression_Dsp, TubeScreamerGoldenFile48k)
{
    EXPECT_LE(SushiDSP::golden::compare_or_regenerate("tube_screamer_48k", render_tube_screamer(48000.0)), -80.0);
}

// Expected red until Task 8 re-pins against the DK chain at 4x - see the D6c plan.
TEST(Regression_Dsp, TubeScreamerGoldenFile96k)
{
    EXPECT_LE(SushiDSP::golden::compare_or_regenerate("tube_screamer_96k", render_tube_screamer(96000.0)), -80.0);
}
