/**************************************************************************/
/* test_antiderivative_antialiaser.cpp                                    */
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
 * @file test_antiderivative_antialiaser.cpp
 * @brief Tests the first-order antiderivative antialiaser: finite output, its bounds and its reset.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include "support/tolerance.hpp"

#include <SushiDSP/foundation/circuits/stages/antiderivative_antialiaser.hpp>
#include <SushiDSP/foundation/circuits/stages/grid_conduction_law.hpp>
#include <SushiDSP/foundation/circuits/stages/plate_swing.hpp>

#include <cmath>
#include <string>

namespace
{
    constexpr float K_SUPPLY_HEADROOM_VOLTS = 150.0f;
    constexpr float K_BOTTOM_HEADROOM_VOLTS = 100.0f;
    constexpr float K_GAIN = 20.0f;

    const SushiDSP::GridConductionLaw K_GRID{1.0e-6f, 0.5f, 0.15f};
    const SushiDSP::PlateSwing K_PLATE{K_SUPPLY_HEADROOM_VOLTS, K_BOTTOM_HEADROOM_VOLTS};

    SushiDSP::AntiderivativeAntialiaser make_stage()
    {
        return SushiDSP::AntiderivativeAntialiaser(1000.0f, 1.0e6f, -2.0f, K_GAIN, K_GRID, K_PLATE);
    }

    /// @brief Returns how far past a headroom the output may read: the cast of the double output to float (one rounding of the headroom) plus the divided difference of the tabulated plate antiderivative.
    ///
    /// The table is a cubic Hermite over knots h = headroom / gain / 64 apart, whose error is h^4 / 384 times the fourth derivative of the antiderivative, at most gain^3 / headroom; dividing the difference of two such values by an input step of at least 0.1 V at these rows gives the second term.
    double swing_slack(double headroom)
    {
        const double u = TestSupport::unit_roundoff<float>();
        const double spacing = headroom / K_GAIN / 64.0;
        const double table = 2.0 * std::pow(spacing, 4.0) / 384.0 * K_GAIN * K_GAIN * K_GAIN / headroom / 0.1;
        return 2.0 * u * headroom + table;
    }

    /// @brief Holds two consecutive excursions and the name its row is reported under; the second sample is the one checked.
    struct Pair
    {
            const char* name;
            float first;
            float second;
    };

    // The rows cover both plate sides, the sign change, the equal-excursion path and a near-coincident pair.
    const Pair kPairs[] = {
        {"DeepSupplySide", -5.0f, -4.9f},   {"NearZeroFalling", -0.1f, 0.0f}, {"NearZeroRising", 0.0f, 0.1f},
        {"DeepBottomSide", 4.9f, 5.0f},     {"WholeSwingUp", -3.0f, 3.0f},    {"WholeSwingDown", 3.0f, -3.0f},
        {"RepeatedExcursion", 0.5f, 0.5f},  {"NearCoincident", 1.0f, 1.00001f},
    };

    std::string pair_name(const ::testing::TestParamInfo<Pair>& info)
    {
        return std::string(info.param.name);
    }
} // namespace

class Small_AntiderivativeAntialiaserOutput : public ::testing::TestWithParam<Pair>
{
};

TEST_P(Small_AntiderivativeAntialiaserOutput, IsFinite)
{
    SushiDSP::AntiderivativeAntialiaser stage = make_stage();
    stage.process(GetParam().first);
    EXPECT_TRUE(std::isfinite(stage.process(GetParam().second)));
}

TEST_P(Small_AntiderivativeAntialiaserOutput, StaysInsideThePlateSwing)
{
    SushiDSP::AntiderivativeAntialiaser stage = make_stage();
    stage.process(GetParam().first);
    const float output = stage.process(GetParam().second);

    EXPECT_LE(output, K_SUPPLY_HEADROOM_VOLTS + swing_slack(K_SUPPLY_HEADROOM_VOLTS));
    EXPECT_GE(output, -K_BOTTOM_HEADROOM_VOLTS - swing_slack(K_BOTTOM_HEADROOM_VOLTS));
}

INSTANTIATE_TEST_SUITE_P(Pairs, Small_AntiderivativeAntialiaserOutput, ::testing::ValuesIn(kPairs), pair_name);

TEST(Small_AntiderivativeAntialiaser, ResetMakesTheNextSampleAFreshTransfer)
{
    SushiDSP::AntiderivativeAntialiaser fresh = make_stage();
    const float first = fresh.process(1.0f);

    SushiDSP::AntiderivativeAntialiaser used = make_stage();
    used.process(-3.0f);
    used.process(0.5f);
    used.reset();
    EXPECT_FLOAT_EQ(used.process(1.0f), first);
}

// A repeated excursion takes the memoised transfer the first sample stored, so the two outputs are the same bits.
TEST(Small_AntiderivativeAntialiaser, ASecondIdenticalSampleGivesTheFirstSamplesOutput)
{
    SushiDSP::AntiderivativeAntialiaser stage = make_stage();
    const float first = stage.process(0.75f);
    const float second = stage.process(0.75f);

    EXPECT_EQ(second, first);
}
