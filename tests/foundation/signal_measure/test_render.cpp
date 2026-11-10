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
/* Licensed under the Apache License, Version 2.0 (the "License"); you    */
/* may not use this file except in compliance with the License. You may   */
/* obtain a copy of the License at                                        */
/*                                                                        */
/*     https://www.apache.org/licenses/LICENSE-2.0                        */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

/**
 * @file test_render.cpp
 * @brief Tests that the renderer streams a stimulus through a node block by block and that the sine stimulus has the requested length and peak.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/core/audio_node.hpp>
#include <SushiDSP/foundation/signal_measure/render.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "support/tolerance.hpp"

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

    /// @brief Returns the values 0, 1, 2, ... for length samples.
    std::vector<float> ramp(std::size_t length)
    {
        std::vector<float> values(length);
        for (std::size_t i = 0; i < length; ++i)
        {
            values[i] = static_cast<float>(i);
        }
        return values;
    }

    /// @brief Returns the largest magnitude in a signal.
    float peak_of(const std::vector<float>& signal)
    {
        float peak = 0.0f;
        for (const float sample : signal)
        {
            peak = std::max(peak, std::fabs(sample));
        }
        return peak;
    }

    /// @brief Rounded operations on the longest path to a sine sample: the double sine, the
    /// product with the amplitude and the conversion to float.
    constexpr std::size_t K_SINE_OPERATIONS = 3;
} // namespace

TEST(Small_Render, EverySampleOfAPartialLastBlockIsProcessed)
{
    GainNode node(0.5f);
    const std::vector<float> stimulus(1000, 1.0f);

    // 0.5 times 1 is exact, so the comparison is equality.
    EXPECT_EQ(SushiDSP::measure::render(node, stimulus, 48000.0, 64), std::vector<float>(1000, 0.5f));
}

TEST(Small_Render, OutputKeepsTheStimulusOrder)
{
    GainNode node(1.0f);
    const std::vector<float> stimulus = ramp(300);

    EXPECT_EQ(SushiDSP::measure::render(node, stimulus, 48000.0, 128), stimulus);
}

TEST(Small_Render, SineStimulusHasTheRequestedLength)
{
    EXPECT_EQ(SushiDSP::measure::sine_stimulus(1000.0, 48000.0, 4800, 0.25f).size(), 4800u);
}

TEST(Small_Render, SineStimulusHasTheRequestedPeak)
{
    // 1000 Hz at 48000 Hz is 48 samples per cycle, so sample 12 lands on the crest.
    const std::vector<float> sine = SushiDSP::measure::sine_stimulus(1000.0, 48000.0, 4800, 0.25f);

    EXPECT_TRUE(TestSupport::is_within(
        TestSupport::measure_relative_error<float>(peak_of(sine), 0.25, K_SINE_OPERATIONS)));
}
