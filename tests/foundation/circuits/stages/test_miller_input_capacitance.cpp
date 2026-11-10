/**************************************************************************/
/* test_miller_input_capacitance.cpp                                      */
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
 * @file test_miller_input_capacitance.cpp
 * @brief The 12AX7 grid capacitance preset's figures and the pole it makes.
 * @author Mustafa Garip & Sushi Systems
 */

#include <cmath>
#include <cstddef>
#include <string>

#include <gtest/gtest.h>

#include "support/tolerance.hpp"

#include <SushiDSP/foundation/circuits/stages/miller_input_capacitance.hpp>

namespace
{
    constexpr double K_PI = 3.14159265358979323846;

    /// @brief Holds the rounding budget of the corner formula: a sum, a product, a scaling, a product of three and a quotient.
    constexpr std::size_t K_CORNER_OPERATIONS = 8;

    double corner_by_hand(const SushiDSP::MillerInputCapacitance& c, double gain, double ohms)
    {
        const double farads = (c.grid_cathode_picofarads + c.grid_plate_picofarads * (1.0 + gain) + c.stray_wiring_picofarads) * 1e-12;
        return 1.0 / (2.0 * K_PI * ohms * farads);
    }

    /// @brief Holds one corner row: a capacitance set, a stage gain and a driving resistance.
    struct CornerRow
    {
            const char* name;
            SushiDSP::MillerInputCapacitance capacitance;
            double gain;
            double ohms;
    };

    // The first two rows are the figures Jcm800 reads for V1a and V1b; the last is the zero-gain sum with other capacitances.
    const CornerRow kCornerRows[] = {
        {"FirstStageGain", SushiDSP::K_TRIODE12_AX7_MILLER_CAPACITANCE, 55.432497, 100000.0},
        {"SecondStageGain", SushiDSP::K_TRIODE12_AX7_MILLER_CAPACITANCE, 7.774305, 100000.0},
        {"ZeroGainIsTheUnmultipliedSum", SushiDSP::MillerInputCapacitance{2.0f, 3.0f, 5.0f}, 0.0, 100000.0},
    };
} // namespace

class Small_MillerInputCapacitanceCorner : public ::testing::TestWithParam<CornerRow>
{
};

TEST_P(Small_MillerInputCapacitanceCorner, MatchesTheHandComputedPole)
{
    const CornerRow& row = GetParam();

    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_relative_error<double>(
        row.capacitance.corner_hz(row.gain, row.ohms), corner_by_hand(row.capacitance, row.gain, row.ohms),
        K_CORNER_OPERATIONS)));
}

INSTANTIATE_TEST_SUITE_P(Rows, Small_MillerInputCapacitanceCorner, ::testing::ValuesIn(kCornerRows),
                         [](const ::testing::TestParamInfo<CornerRow>& info) { return std::string(info.param.name); });

TEST(Small_MillerInputCapacitance, TheCornerFallsAsTheStageGainRises)
{
    const SushiDSP::MillerInputCapacitance& c = SushiDSP::K_TRIODE12_AX7_MILLER_CAPACITANCE;
    EXPECT_LT(c.corner_hz(70.0, 100000.0), c.corner_hz(10.0, 100000.0));
}

TEST(Small_MillerInputCapacitance, TheCornerFallsAsTheDrivingResistanceRises)
{
    const SushiDSP::MillerInputCapacitance& c = SushiDSP::K_TRIODE12_AX7_MILLER_CAPACITANCE;
    EXPECT_LT(c.corner_hz(70.0, 220000.0), c.corner_hz(70.0, 100000.0));
}
