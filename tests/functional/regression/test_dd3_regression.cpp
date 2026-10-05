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

// _USE_MATH_DEFINES precedes every include so that MSVC's <cmath> declares M_PI.
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

    /// @brief Renders one second of the pedal at the knob positions the snapshot pins.
    /// @param sample_rate Rate to render at.
    /// @return sample_rate samples of output.
    /// @see tests/functional/regression/README.md
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

/// @brief Tests that the 48 kHz render matches its golden file within -80 dB.
/// @see tests/functional/regression/README.md
TEST(Regression_Dsp, Dd3GoldenFile48k)
{
    EXPECT_LE(SushiDSP::golden::compare_or_regenerate("dd3_48k", render_dd3(48000.0)), -80.0);
}

TEST(Regression_Dsp, Dd3GoldenFile96k)
{
    EXPECT_LE(SushiDSP::golden::compare_or_regenerate("dd3_96k", render_dd3(96000.0)), -80.0);
}
