/**************************************************************************/
/* tone_stack_contract.hpp                                                */
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
 * @file tone_stack_contract.hpp
 * @brief The checks every amplifier tone stack keeps, written once for the stacks that run them.
 * @author Mustafa Garip
 */

#pragma once

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <vector>

#include <SushiDSP/foundation/signal_measure/render.hpp>

namespace TestSupport
{
    /// @brief Fails when a stack answers silence with anything but exact zero.
    ///
    /// The stack is linear, so zero state and zero input give exact zero.
    /// @tparam Stack A tone stack with reset() and process_sample(float).
    /// @param stack The stack under test, knobs already set.
    template <typename Stack>
    void expect_silence_gives_silence(Stack& stack)
    {
        stack.reset();
        EXPECT_EQ(stack.process_sample(0.0f), 0.0f);
    }

    /// @brief Fails for every sample of a sine through a stack that is not a finite number.
    /// @tparam Stack A tone stack with reset() and process_sample(float).
    /// @param stack The stack under test, knobs already set.
    /// @param frequency_hz Frequency of the sine.
    /// @param sample_rate The rate the stack was built for.
    /// @param samples Length of the run.
    /// @param amplitude Peak of the sine.
    template <typename Stack>
    void expect_finite_on_a_sine(Stack& stack, double frequency_hz, double sample_rate, std::size_t samples,
                                 float amplitude)
    {
        stack.reset();
        const std::vector<float> input = SushiDSP::measure::sine_stimulus(frequency_hz, sample_rate, samples, amplitude);
        for (std::size_t i = 0; i < input.size(); ++i)
        {
            ASSERT_TRUE(std::isfinite(stack.process_sample(input[i]))) << i;
        }
    }
} // namespace TestSupport
