/**************************************************************************/
/* test_output.cpp                                                        */
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
 * @file test_output.cpp
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
#include <SushiDSP/foundation/regression_test/regression_test.hpp>

#include <SushiDSP/nodes/dd3/dd3_delay.hpp>
#include <SushiDSP/foundation/golden_compare/golden_file.hpp>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{
    constexpr std::uint32_t K_BLOCK = 256u;

    /// @brief Renders one second of the pedal at the knob positions the snapshot pins.
    /// @param sample_rate Rate to render at.
    /// @return sample_rate samples of output.
    /// @see tests/nodes/dd3/test_dd3_delay/README.md
    std::vector<float> render_dd3(double sample_rate)
    {
        SushiDSP::Dd3Delay node;
        node.set_mode(SushiDSP::dd3::Dd3Mode::Middle);
        node.set_time(0.4f);
        node.set_feedback(0.45f);
        node.set_level(0.5f);
        node.prepare({sample_rate, K_BLOCK, 1});
        const auto input = SushiDSP::golden::stimulus(sample_rate);
        std::vector<float> output(input.size(), 0.0f);
        for (std::size_t position = 0; position < input.size(); position += K_BLOCK)
        {
            const std::uint32_t frames =
                static_cast<std::uint32_t>(std::min<std::size_t>(K_BLOCK, input.size() - position));
            const float* inputs[1] = {input.data() + position};
            float* outputs[1] = {output.data() + position};
            SushiDSP::ProcessContext context{outputs, inputs, frames, 0.0};
            node.process(context);
        }
        return output;
    }
} // namespace

/// @brief Tests that the 48 kHz render matches its golden file within -80 dB.
/// @see tests/nodes/dd3/test_dd3_delay/README.md
SUSHIDSP_REGRESSION_TEST(Medium_Dd3Delay, Dd3GoldenFile48k)
{
    EXPECT_LE(SushiDSP::golden::compare_or_regenerate(SUSHIDSP_GOLDEN_DIR, "dd3_48k", render_dd3(48000.0)), -80.0);
}

SUSHIDSP_REGRESSION_TEST(Medium_Dd3Delay, Dd3GoldenFile96k)
{
    EXPECT_LE(SushiDSP::golden::compare_or_regenerate(SUSHIDSP_GOLDEN_DIR, "dd3_96k", render_dd3(96000.0)), -80.0);
}
