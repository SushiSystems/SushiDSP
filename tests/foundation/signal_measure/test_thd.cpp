/**************************************************************************/
/* test_thd.cpp                                                           */
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
 * @file test_thd.cpp
 * @brief Tests the total harmonic distortion of a pure tone and of a cubic, and the shape of its harmonic table.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/core/audio_node.hpp>
#include <SushiDSP/foundation/signal_measure/render.hpp>
#include <SushiDSP/foundation/signal_measure/spectrum.hpp>
#include <SushiDSP/foundation/signal_measure/thd.hpp>

#include <cmath>
#include <cstdint>
#include <vector>

namespace
{
    /// @brief y = x - x^3/3: third harmonic (A^3/12) / (A - A^3/4) of the fundamental.
    class CubicNode : public SushiDSP::AudioNode
    {
        public:
            /// @brief Holds no state, so there is nothing to prepare.
            void prepare(const SushiDSP::PrepareInfo&) override
            {
            }

            /// @brief Applies the transfer to every frame of the block.
            void process(SushiDSP::ProcessContext& context) override
            {
                for (std::uint32_t i = 0; i < context.frames; ++i)
                {
                    const float x = context.inputs[0][i];
                    context.outputs[0][i] = x - x * x * x / 3.0f;
                }
            }

            /// @brief Reports one input channel and one output channel.
            SushiDSP::PortLayout ports() const override
            {
                return {1, 1};
            }

            /// @brief Exposes no parameter, so every read is zero.
            float get_parameter(const char*) const override
            {
                return 0.0f;
            }

            /// @brief Exposes no parameter, so every write is ignored.
            void set_parameter_for_control_thread(const char*, float) override
            {
            }

            /// @brief Holds no state, so there is nothing to clear.
            void reset() override
            {
            }
    };
} // namespace

TEST(Small_Thd, PureToneHasNegligibleDistortion)
{
    const std::vector<float> tone = SushiDSP::measure::sine_stimulus(1000.0, 48000.0,
                                                                     SushiDSP::measure::K_FFT_SIZE, 0.5f);
    const SushiDSP::measure::ThdResult result = SushiDSP::measure::thd(tone, 1000.0, 48000.0);
    EXPECT_LT(result.thd_percent, 0.01);
}

TEST(Small_Thd, PureSineOnAnExactBinIsNegligible)
{
    const double sample_rate = 48000.0;
    const double f0 =
        SushiDSP::measure::bin_exact_frequency(1000.0, sample_rate, SushiDSP::measure::K_FFT_SIZE);
    const std::vector<float> signal =
        SushiDSP::measure::sine_stimulus(f0, sample_rate, SushiDSP::measure::K_FFT_SIZE, 0.5f);
    const SushiDSP::measure::ThdResult result = SushiDSP::measure::thd(signal, f0, sample_rate);
    EXPECT_LT(result.thd_percent, 1e-3);
    ASSERT_EQ(result.per_harmonic_db.size(), 20u);
    EXPECT_LT(result.per_harmonic_db[1], -100.0);
}

TEST(Small_Thd, CubicMatchesAnalyticThirdHarmonic)
{
    // A = 0.5: third harmonic 0.010417 / 0.46875 = 2.2222 % of the fundamental.
    const double sample_rate = 48000.0;
    const double f0 =
        SushiDSP::measure::bin_exact_frequency(1000.0, sample_rate, SushiDSP::measure::K_FFT_SIZE);
    CubicNode node;
    const std::vector<float> output = SushiDSP::measure::render(
        node, SushiDSP::measure::sine_stimulus(f0, sample_rate, SushiDSP::measure::K_FFT_SIZE, 0.5f),
        sample_rate, 256);
    const SushiDSP::measure::ThdResult result = SushiDSP::measure::thd(output, f0, sample_rate);
    EXPECT_NEAR(result.thd_percent, 2.2222, 0.01);
    EXPECT_NEAR(result.per_harmonic_db[2], 20.0 * std::log10(0.022222), 0.05);
}

TEST(Small_Thd, ReportsOneEntryPerRequestedHarmonic)
{
    const std::vector<float> tone = SushiDSP::measure::sine_stimulus(1000.0, 48000.0,
                                                                     SushiDSP::measure::K_FFT_SIZE, 0.5f);
    const SushiDSP::measure::ThdResult result = SushiDSP::measure::thd(tone, 1000.0, 48000.0, 5);
    EXPECT_EQ(result.per_harmonic_db.size(), 5u);
    EXPECT_NEAR(result.per_harmonic_db[0], 0.0, 1e-9);
}

TEST(Small_Thd, ShortSignalIsRefusedAsZero)
{
    const std::vector<float> short_signal(100, 0.1f);
    const SushiDSP::measure::ThdResult result = SushiDSP::measure::thd(short_signal, 1000.0, 48000.0);
    EXPECT_EQ(result.thd_percent, 0.0);
}
