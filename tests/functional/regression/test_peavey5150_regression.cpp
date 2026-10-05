/****************************************************************/
/* test_peavey5150_regression.cpp                               */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file test_peavey5150_regression.cpp
 * @brief Tests the Peavey5150 node against its golden files.
 * @author Mustafa Garip
 */

// _USE_MATH_DEFINES precedes every include so that MSVC's <cmath> declares M_PI.
#define _USE_MATH_DEFINES

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "SushiDSP/peavey5150/Peavey5150.hpp"
#include "golden/GoldenFile.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{
    std::vector<float> render_peavey5150(double sample_rate)
    {
        SushiDSP::Peavey5150 node;
        // Knobs before prepare(): the smoothers snap, so the file holds the settled amp.
        node.set_pre_gain(0.6f);
        node.set_low(0.6f);
        node.set_mid(0.4f);
        node.set_high(0.7f);
        node.set_post_gain(0.0f);
        node.set_resonance(0.5f);
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

/// @brief Tests that the 48 kHz render matches its golden file within -80 dB.
/// @see tests/functional/regression/README.md
TEST(Regression_Dsp, Peavey5150GoldenFile48k)
{
    EXPECT_LE(SushiDSP::golden::compare_or_regenerate("peavey5150_48k", render_peavey5150(48000.0)), -80.0);
}

TEST(Regression_Dsp, Peavey5150GoldenFile96k)
{
    EXPECT_LE(SushiDSP::golden::compare_or_regenerate("peavey5150_96k", render_peavey5150(96000.0)), -80.0);
}
