/**************************************************************************/
/* test_mesa_mark2c_tone_stack.cpp                                        */
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
 * @file test_mesa_mark2c_tone_stack.cpp
 * @brief Tests that the Mesa tone stack is silent from silence and finite with the bright switch on.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <SushiDSP/nodes/mesa_mark2c/mesa_mark2c_tone_stack.hpp>

#include "support/tone_stack_contract.hpp"

namespace
{
    constexpr double K_SAMPLE_RATE = 48000.0;
} // namespace

TEST(Small_MesaMark2cToneStack, SilenceGivesSilence)
{
    SushiDSP::MesaMark2cToneStack stack(K_SAMPLE_RATE);
    TestSupport::expect_silence_gives_silence(stack);
}

TEST(Small_MesaMark2cToneStack, BrightSwitchKeepsTheOutputFinite)
{
    SushiDSP::MesaMark2cToneStack stack(K_SAMPLE_RATE);
    stack.set_bright(true);
    stack.set_treble(0.9f);
    TestSupport::expect_finite_on_a_sine(stack, 2000.0, K_SAMPLE_RATE, 4800u, 0.1f);
}
