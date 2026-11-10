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
 * @file test_fir_filter.cpp
 * @brief Tests that a FIR filter loaded with a unit impulse passes its input unchanged.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/nodes/room/fir_filter.hpp>

#include <array>
#include <vector>

TEST(Small_FirFilter, UnitImpulseTapPassesTheInputUnchanged)
{
    // A tap of one at lag zero and zeros elsewhere multiplies by one and adds zeros, which is exact.
    std::vector<float> taps(SushiDSP::room::FirFilter::K_TAPS, 0.0f);
    taps[0] = 1.0f;
    SushiDSP::room::FirFilter filter;
    filter.prepare(8u);
    filter.load(taps.data());
    filter.reset();
    const std::array<float, 8> input = {1.0f, 0.0f, 0.0f, -0.5f, 0.0f, 0.0f, 0.0f, 0.0f};
    std::array<float, 8> output{};

    filter.process(input.data(), output.data(), 8u);

    EXPECT_EQ(output, input);
}
