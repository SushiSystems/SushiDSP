/**************************************************************************/
/* test_frequency_response.cpp                                            */
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
 * @file test_frequency_response.cpp
 * @brief Tests that a unity-gain node measures as flat, and that its gain shows as a level offset.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/core/audio_node.hpp>
#include <SushiDSP/foundation/signal_measure/frequency_response.hpp>

#include <cmath>
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

TEST(FrequencyResponse, UnityGainIsFlatAcrossTheBand)
{
    GainNode node(1.0f);
    const std::vector<SushiDSP::measure::ResponsePoint> points =
        SushiDSP::measure::frequency_response(node, 48000.0, {100.0, 1000.0, 5000.0});
    ASSERT_EQ(points.size(), 3u);
    for (const SushiDSP::measure::ResponsePoint& point : points)
    {
        EXPECT_NEAR(point.magnitude_db, 0.0, 0.5);
    }
}

TEST(FrequencyResponse, HalfGainReadsAsMinusSixDecibels)
{
    GainNode node(0.5f);
    const std::vector<SushiDSP::measure::ResponsePoint> points =
        SushiDSP::measure::frequency_response(node, 48000.0, {1000.0});
    ASSERT_EQ(points.size(), 1u);
    EXPECT_NEAR(points[0].magnitude_db, 20.0 * std::log10(0.5), 0.5);
}
