/****************************************************************/
/* test_mesa_mark2c_regression.cpp                              */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file test_mesa_mark2c_regression.cpp
 * @brief Tests the MesaMark2c node against its golden files.
 * @author Mustafa Garip
 */

// _USE_MATH_DEFINES precedes every include so that MSVC's <cmath> declares M_PI.
#define _USE_MATH_DEFINES

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "SushiDSP/mesa_mark2c/MesaMark2c.hpp"
#include "golden/GoldenFile.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{
    std::vector<float> render_mesa_mark2c(double sample_rate)
    {
        SushiDSP::MesaMark2c node;
        // Knobs before prepare(): the smoothers snap, so the file holds the settled amp.
        node.set_volume_1(0.6f);
        node.set_treble(0.7f);
        node.set_bass(0.4f);
        node.set_middle(0.5f);
        node.set_lead_drive(0.7f);
        node.set_lead_master(0.5f);
        node.set_master_1(0.0f);
        node.set_presence(0.5f);
        for (std::size_t band = 0; band < SushiDSP::MesaMark2c::kEqualizerBandCount; ++band)
        {
            node.set_equalizer_band(band, 0.0f);
        }
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
TEST(Regression_Dsp, MesaMark2cGoldenFile48k)
{
    EXPECT_LE(SushiDSP::golden::compare_or_regenerate("mesa_mark2c_48k", render_mesa_mark2c(48000.0)), -80.0);
}

TEST(Regression_Dsp, MesaMark2cGoldenFile96k)
{
    EXPECT_LE(SushiDSP::golden::compare_or_regenerate("mesa_mark2c_96k", render_mesa_mark2c(96000.0)), -80.0);
}
