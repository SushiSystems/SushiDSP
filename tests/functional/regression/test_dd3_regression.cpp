/****************************************************************/
/* test_dd3_regression.cpp                                      */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file test_dd3_regression.cpp
 * @brief Golden-vector regression tests for the Dd3Delay node.
 * @author Mustafa Garip & Sushi Systems
 */

// _USE_MATH_DEFINES must precede every include: it is what makes MSVC's <cmath> declare M_PI,
// and <cmath> may otherwise be pulled in transitively before this point.
#define _USE_MATH_DEFINES

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "SushiDSP/dd3/Dd3Delay.hpp"
#include "golden/GoldenFile.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{
    constexpr std::uint32_t kBlock = 256u;

    /// @brief One second of the pedal at the knob positions the snapshot pins.
    ///
    /// MODE M at D.TIME 0.4 clocks the ring at 46804.2 Hz, so 4096 samples of it are 87.5 ms
    /// and the whole wet path is 89.1 ms, several repeats inside the stimulus. F.BACK 0.45 sits
    /// inside the loop's stable region while still recirculating: a change to either resampler,
    /// to the compander or to the converter moves the file. Every knob is set before prepare()
    /// so the smoothers snap and the file holds the settled pedal rather than its ramp.
    /// @param sample_rate Rate to render at.
    /// @return sample_rate samples of output.
    std::vector<float> render_dd3(double sample_rate)
    {
        SushiDSP::Dd3Delay node;
        node.set_mode(SushiDSP::dd3::Dd3Mode::Middle);
        node.set_time(0.4f);
        node.set_feedback(0.45f);
        node.set_level(0.5f);
        node.prepare({ sample_rate, kBlock, 1 });
        const auto input = SushiDSP::golden::stimulus(sample_rate);
        std::vector<float> output(input.size(), 0.0f);
        for (std::size_t position = 0; position < input.size(); position += kBlock)
        {
            const std::uint32_t frames =
                static_cast<std::uint32_t>(std::min<std::size_t>(kBlock, input.size() - position));
            const float* inputs[1] = { input.data() + position };
            float* outputs[1] = { output.data() + position };
            SushiDSP::ProcessContext context{ outputs, inputs, frames, 0.0 };
            node.process(context);
        }
        return output;
    }
}

// One second of the pedal's output is pinned as tests/golden/dd3_<rate>.f32 and compared by RMS
// residual. -80 dB catches any real behaviour change and absorbs float reassociation between
// toolchains.
//
// Regenerate deliberately, never to make a red test green:
//   SUSHIDSP_REGENERATE_GOLDEN=1 sd test -s regression
// and say why in the commit message and in tests/golden/README.md.
TEST(Regression_Dsp, Dd3GoldenFile48k)
{
    EXPECT_LE(SushiDSP::golden::compare_or_regenerate("dd3_48k", render_dd3(48000.0)), -80.0);
}

TEST(Regression_Dsp, Dd3GoldenFile96k)
{
    EXPECT_LE(SushiDSP::golden::compare_or_regenerate("dd3_96k", render_dd3(96000.0)), -80.0);
}
