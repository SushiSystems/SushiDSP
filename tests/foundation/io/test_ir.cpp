/**************************************************************************/
/* test_ir.cpp                                                            */
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
 * @file test_ir.cpp
 * @brief Tests that an impulse response holds its samples and its sample rate.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/io/ir.hpp>

#include <vector>

TEST(Small_Ir, DefaultIsEmptyWithNoSampleRate)
{
    const SushiDSP::Ir ir;
    EXPECT_TRUE(ir.samples.empty());
    EXPECT_EQ(ir.sample_rate, 0.0);
}

TEST(Small_Ir, HoldsTheSamplesAndRateItWasGiven)
{
    SushiDSP::Ir ir;
    ir.samples = std::vector<float>{1.0f, 0.25f, -0.5f};
    ir.sample_rate = 48000.0;
    EXPECT_EQ(ir.samples.size(), 3u);
    EXPECT_FLOAT_EQ(ir.samples[2], -0.5f);
    EXPECT_EQ(ir.sample_rate, 48000.0);
}
