/**************************************************************************/
/* test_peavey5150_tone_stack.cpp                                         */
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
 * @file test_peavey5150_tone_stack.cpp
 * @brief Tests that the Peavey tone stack is silent from silence and finite across its controls.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <SushiDSP/nodes/peavey5150/peavey5150_tone_stack.hpp>

#include "support/tone_stack_contract.hpp"

namespace
{
    constexpr double K_SAMPLE_RATE = 48000.0;
} // namespace

TEST(Small_Peavey5150ToneStack, SilenceGivesSilence)
{
    SushiDSP::Peavey5150ToneStack stack(K_SAMPLE_RATE);
    TestSupport::expect_silence_gives_silence(stack);
}

TEST(Small_Peavey5150ToneStack, ToneStaysFiniteAcrossTheControls)
{
    SushiDSP::Peavey5150ToneStack stack(K_SAMPLE_RATE);
    stack.set_low(0.7f);
    stack.set_mid(0.3f);
    stack.set_high(0.5f);
    TestSupport::expect_finite_on_a_sine(stack, 500.0, K_SAMPLE_RATE, 4800u, 0.1f);
}
