/**************************************************************************/
/* test_fir_filter.cpp                                                    */
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
 * @file test_fir_filter.cpp
 * @brief Tests that a FIR filter loaded with a unit impulse passes its input unchanged.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/nodes/room/fir_filter.hpp>
#include <SushiDSP/nodes/room/hrtf_set.hpp>

#include <array>
#include <vector>

TEST(FirFilter, UnitImpulseTapPassesTheInputUnchanged)
{
    std::vector<float> taps(SushiDSP::room::FirFilter::K_TAPS, 0.0f);
    taps[0] = 1.0f;
    SushiDSP::room::FirFilter filter;
    filter.prepare(8u);
    filter.load(taps.data());
    filter.reset();
    std::array<float, 8> input{};
    input[0] = 1.0f;
    input[3] = -0.5f;
    std::array<float, 8> output{};
    filter.process(input.data(), output.data(), 8u);
    for (std::size_t i = 0; i < input.size(); ++i)
    {
        EXPECT_NEAR(output[i], input[i], 1e-6f);
    }
}

TEST(FirFilter, TapCountIsTheHrtfTapCount)
{
    EXPECT_EQ(SushiDSP::room::FirFilter::K_TAPS, SushiDSP::room::HrtfSet::K_TAP_COUNT);
}
