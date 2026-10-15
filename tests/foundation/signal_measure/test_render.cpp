/**************************************************************************/
/* test_render.cpp                                                        */
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
 * @file test_render.cpp
 * @brief Tests that the renderer streams a stimulus through a node block by block and that the sine stimulus has the requested length and peak.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/core/audio_node.hpp>
#include <SushiDSP/foundation/signal_measure/render.hpp>

#include <cmath>
#include <cstdint>
#include <vector>

namespace
{
    /// @brief y = g x, one input and one output, the reference for the measurement tests.
    class GainNode : public SushiDSP::AudioNode
    {
        public:
            explicit GainNode(float gain) : gain_(gain) {}

            void prepare(const SushiDSP::PrepareInfo&) override {}

            void process(SushiDSP::ProcessContext& context) override
            {
                for (std::uint32_t i = 0; i < context.frames; ++i)
                {
                    context.outputs[0][i] = gain_ * context.inputs[0][i];
                }
            }

            SushiDSP::PortLayout ports() const override
            {
                return {1, 1};
            }

            float get_parameter(const char*) const override
            {
                return 0.0f;
            }
            void set_parameter_for_control_thread(const char*, float) override {}

            void reset() override {}

        private:
            float gain_;
    };
} // namespace

TEST(Render, EverySampleOfAPartialLastBlockIsProcessed)
{
    GainNode node(0.5f);
    const std::vector<float> stimulus(1000, 1.0f);
    const std::vector<float> output = SushiDSP::measure::render(node, stimulus, 48000.0, 64);
    ASSERT_EQ(output.size(), stimulus.size());
    for (float sample : output)
    {
        EXPECT_FLOAT_EQ(sample, 0.5f);
    }
}

TEST(Render, OutputKeepsTheStimulusOrder)
{
    GainNode node(1.0f);
    std::vector<float> stimulus(300);
    for (std::size_t i = 0; i < stimulus.size(); ++i)
    {
        stimulus[i] = static_cast<float>(i);
    }
    const std::vector<float> output = SushiDSP::measure::render(node, stimulus, 48000.0, 128);
    EXPECT_EQ(output, stimulus);
}

TEST(Render, SineStimulusHasTheRequestedLengthAndPeak)
{
    const std::vector<float> sine = SushiDSP::measure::sine_stimulus(1000.0, 48000.0, 4800, 0.25f);
    ASSERT_EQ(sine.size(), 4800u);
    float peak = 0.0f;
    for (float sample : sine)
    {
        peak = std::max(peak, std::fabs(sample));
    }
    EXPECT_NEAR(peak, 0.25f, 1e-3f);
}
