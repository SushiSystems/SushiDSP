/**************************************************************************/
/* test_diezel_vh4_tone_stack.cpp                                         */
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
 * @file test_diezel_vh4_tone_stack.cpp
 * @brief Tests that the Diezel tone stack is silent from silence, finite on a tone and never amplifies its input.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <SushiDSP/nodes/diezel_vh4/diezel_vh4_tone_stack.hpp>
#include <SushiDSP/foundation/signal_measure/render.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include "support/tolerance.hpp"
#include "support/tone_stack_contract.hpp"

namespace
{
    constexpr double K_SAMPLE_RATE = 48000.0;

    /// @brief The sample the stack's gain is read from, past the startup transient.
    constexpr std::size_t K_SETTLED_FROM = 2400u;

    /// @brief The largest magnitude of a sine through the stack from `K_SETTLED_FROM` on.
    float settled_peak(SushiDSP::DiezelVh4ToneStack& stack, double frequency_hz, std::size_t samples, float amplitude)
    {
        const std::vector<float> input = SushiDSP::measure::sine_stimulus(frequency_hz, K_SAMPLE_RATE, samples, amplitude);
        float peak = 0.0f;
        for (std::size_t i = 0; i < input.size(); ++i)
        {
            const float output = stack.process_sample(input[i]);
            if (i >= K_SETTLED_FROM)
            {
                peak = std::max(peak, std::fabs(output));
            }
        }
        return peak;
    }
} // namespace

TEST(Small_DiezelVh4ToneStack, SilenceGivesSilence)
{
    SushiDSP::DiezelVh4ToneStack stack(K_SAMPLE_RATE);
    TestSupport::expect_silence_gives_silence(stack);
}

TEST(Small_DiezelVh4ToneStack, ToneStaysFiniteAcrossTheControls)
{
    SushiDSP::DiezelVh4ToneStack stack(K_SAMPLE_RATE);
    stack.set_bass(0.8f);
    stack.set_middle(0.2f);
    stack.set_treble(0.6f);
    TestSupport::expect_finite_on_a_sine(stack, 1000.0, K_SAMPLE_RATE, 4800u, 0.1f);
}

TEST(Small_DiezelVh4ToneStack, ToneStackDoesNotAmplifyItsInputAtMidband)
{
    // The stack is passive, so R40's divider bounds the whole path. See tests/nodes/diezel_vh4/test_diezel_vh4/README.md.
    SushiDSP::DiezelVh4ToneStack stack(K_SAMPLE_RATE);
    constexpr float amplitude = 0.1f;
    constexpr std::size_t rounding_operations = 8u;
    const double bound = static_cast<double>(SushiDSP::DiezelVh4ToneStack::input_resistor_attenuation()) *
                         amplitude * (1.0 + rounding_operations * TestSupport::unit_roundoff<float>());
    EXPECT_LE(static_cast<double>(settled_peak(stack, 1000.0, 4800u, amplitude)), bound);
}
