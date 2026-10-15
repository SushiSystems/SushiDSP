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
 * @file test_peavey5150_tone_stack.cpp
 * @brief Tests that the 5150 tone stack is silent from silence and finite on a tone.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/nodes/peavey5150/peavey5150_tone_stack.hpp>

#include <cmath>

TEST(Peavey5150ToneStack, SilenceGivesSilence)
{
    SushiDSP::Peavey5150ToneStack stack(48000.0);
    stack.reset();
    EXPECT_NEAR(stack.process_sample(0.0f), 0.0f, 1e-6f);
}

TEST(Peavey5150ToneStack, ToneStaysFiniteAcrossTheControls)
{
    SushiDSP::Peavey5150ToneStack stack(48000.0);
    stack.set_low(0.7f);
    stack.set_mid(0.3f);
    stack.set_high(0.5f);
    stack.reset();
    for (int i = 0; i < 4800; ++i)
    {
        const float input = 0.1f * std::sin(2.0f * 3.14159265f * 500.0f * static_cast<float>(i) / 48000.0f);
        EXPECT_TRUE(std::isfinite(stack.process_sample(input)));
    }
}
