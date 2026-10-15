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
 * @file test_diezel_vh4_tone_stack.cpp
 * @brief Tests that the Diezel tone stack is silent from silence, finite on a tone and attenuates its input.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/nodes/diezel_vh4/diezel_vh4_tone_stack.hpp>

#include <cmath>

TEST(Small_DiezelVh4ToneStack, SilenceGivesSilence)
{
    SushiDSP::DiezelVh4ToneStack stack(48000.0);
    stack.reset();
    EXPECT_NEAR(stack.process_sample(0.0f), 0.0f, 1e-6f);
}

TEST(Small_DiezelVh4ToneStack, ToneStaysFiniteAcrossTheControls)
{
    SushiDSP::DiezelVh4ToneStack stack(48000.0);
    stack.set_bass(0.8f);
    stack.set_middle(0.2f);
    stack.set_treble(0.6f);
    stack.reset();
    for (int i = 0; i < 4800; ++i)
    {
        const float input = 0.1f * std::sin(2.0f * 3.14159265f * 1000.0f * static_cast<float>(i) / 48000.0f);
        EXPECT_TRUE(std::isfinite(stack.process_sample(input)));
    }
}

TEST(Small_DiezelVh4ToneStack, InputResistorAttenuatesWithinUnity)
{
    EXPECT_GT(SushiDSP::DiezelVh4ToneStack::input_resistor_attenuation(), 0.0f);
    EXPECT_LE(SushiDSP::DiezelVh4ToneStack::input_resistor_attenuation(), 1.0f);
}
