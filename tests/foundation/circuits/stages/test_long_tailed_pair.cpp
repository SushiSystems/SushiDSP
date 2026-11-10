/**************************************************************************/
/* test_long_tailed_pair.cpp                                              */
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
 * @file test_long_tailed_pair.cpp
 * @brief Checks the pair's split, its rejection of a common-mode move and its plate bounds.
 * @author Mustafa Garip & Sushi Systems
 */

#include <cmath>
#include <string>

#include <gtest/gtest.h>

#include "support/tolerance.hpp"

#include <SushiDSP/foundation/circuits/stages/long_tailed_pair.hpp>

namespace
{
    constexpr double K_MU = 96.378299;
    constexpr double K_PLATE_RESISTANCE_OHMS = 34061.086;
    constexpr double K_TAIL_OHMS = 26800.0 + 470.0;
    constexpr double K_PLATE_LOAD_A_OHMS = 82500.0;
    constexpr double K_PLATE_LOAD_B_OHMS = 90900.0;

    const double kFloatU = TestSupport::unit_roundoff<float>();

    /// @brief The Mark IIC+'s inverter, as mesa_mark2c.cpp builds it.
    /// @return A pair on S8's 82.5 k and 90.9 k plates over the feedback leg's 27.27 k tail.
    SushiDSP::LongTailedPair mark2c_inverter()
    {
        return SushiDSP::LongTailedPair(
            SushiDSP::LongTailedPairTube{96.378299f, 34061.086f, -1.5f,
                                         SushiDSP::K_TRIODE12_AX7_GRID_CONDUCTION},
            SushiDSP::LongTailedPairHalf{82500.0f, 25000.0f, 100000.0f,
                                         SushiDSP::PlateSwing{106.646f, 144.645f}},
            SushiDSP::LongTailedPairHalf{90900.0f, 2869.2f, 150000.0f,
                                         SushiDSP::PlateSwing{117.503f, 159.365f}},
            26800.0f, 470.0f);
    }

    /// @brief Returns gm Rt / (1 + 2 gm Rt) for the inverter's tube over its 27.27 k tail, in double.
    double coupling_by_hand()
    {
        const double loop = K_MU / K_PLATE_RESISTANCE_OHMS * K_TAIL_OHMS;
        return loop / (1.0 + 2.0 * loop);
    }

    /// @brief Returns a half's common-cathode gain on its own load line, mu Ra / (Ra + ra).
    double half_gain(double plate_load_ohms)
    {
        return K_MU * plate_load_ohms / (plate_load_ohms + K_PLATE_RESISTANCE_OHMS);
    }

    /// @brief Holds one drive of the two grids and the name its row is reported under.
    struct Drive
    {
            const char* name;
            float grid_a;
            float grid_b;
    };

    const Drive kSwingDrives[] = {
        {"BothHardOpposed", 400.0f, -400.0f},
        {"BothHardOpposedReversed", -400.0f, 400.0f},
        {"BothHardUp", 400.0f, 400.0f},
        {"BothHardDown", -400.0f, -400.0f},
        {"AtRest", 0.0f, 0.0f},
    };

    const Drive kResetDrives[] = {
        {"HalfAOnly", 200.0f, 0.0f},
        {"HalfBOnly", 0.0f, 200.0f},
    };

    std::string drive_name(const ::testing::TestParamInfo<Drive>& info)
    {
        return std::string(info.param.name);
    }

    bool inside_their_swings(const SushiDSP::LongTailedPair::Plates& plates)
    {
        return plates.a <= 106.646f && plates.a >= -144.645f && plates.b <= 117.503f && plates.b >= -159.365f;
    }

    bool finite(const SushiDSP::LongTailedPair::Plates& plates)
    {
        return std::isfinite(plates.a) && std::isfinite(plates.b);
    }
} // namespace

// gm Rt / (1 + 2 gm Rt), gm 2.8296 mS into the 27.27 k the feedback leg returns through; the model forms it in float: a quotient, a product, a sum, a product and a quotient.
TEST(Small_LongTailedPair, TheCathodeCouplingIsTheLoopGainOverOnePlusTwiceIt)
{
    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(
        mark2c_inverter().cathode_coupling(), coupling_by_hand(), 8.0 * kFloatU * coupling_by_hand())));
}

TEST(Small_LongTailedPair, TheCathodeFollowsLessThanHalfTheSummedGrids)
{
    EXPECT_LT(mark2c_inverter().cathode_coupling(), 0.5f);
}

TEST(Small_LongTailedPair, LongTailedPairDrivesItsTwoPlatesInOppositeDirections)
{
    SushiDSP::LongTailedPair pair = mark2c_inverter();

    const SushiDSP::LongTailedPair::Plates plates = pair.process_sample(0.05f, 0.0f);

    EXPECT_LT(plates.a, 0.0f);
    EXPECT_GT(plates.b, 0.0f);
    // Matched amplitudes are not asserted. See tests/foundation/circuits/stages/README.md.
    EXPECT_GT(std::fabs(plates.a), 0.0f);
    EXPECT_GT(std::fabs(plates.b), 0.0f);
}

// Both grids moving by v lift the cathode by 2 k v, so each gap sees (1 - 2 k) v at most (the grid dividers only lower it). Each plate then moves by at most its half's gain times that, and the two plates differ by at most the sum; float rounding of k is amplified by 1 / (1 - 2 k) in the gap.
TEST(Small_LongTailedPair, LongTailedPairRejectsACommonModeMove)
{
    constexpr double drive = 0.05;
    SushiDSP::LongTailedPair pair = mark2c_inverter();
    const double coupling = pair.cathode_coupling();
    const double gap = (1.0 - 2.0 * coupling) * drive + 4.0 * kFloatU * drive;
    const double bound = (half_gain(K_PLATE_LOAD_A_OHMS) + half_gain(K_PLATE_LOAD_B_OHMS)) * gap;

    const SushiDSP::LongTailedPair::Plates plates = pair.process_sample(0.05f, 0.05f);

    EXPECT_LE(std::fabs(static_cast<double>(plates.a) - static_cast<double>(plates.b)), bound);
}

class Small_LongTailedPairSwing : public ::testing::TestWithParam<Drive>
{
};

TEST_P(Small_LongTailedPairSwing, PlatesStayFinite)
{
    SushiDSP::LongTailedPair pair = mark2c_inverter();

    EXPECT_TRUE(finite(pair.process_sample(GetParam().grid_a, GetParam().grid_b)));
    EXPECT_TRUE(finite(pair.process_sample(GetParam().grid_a, GetParam().grid_b)));
}

// The second sample repeats the first, so it takes the antialiaser's repeated-input path as well.
TEST_P(Small_LongTailedPairSwing, PlatesStayInsideTheirOwnSwings)
{
    SushiDSP::LongTailedPair pair = mark2c_inverter();

    EXPECT_TRUE(inside_their_swings(pair.process_sample(GetParam().grid_a, GetParam().grid_b)));
    EXPECT_TRUE(inside_their_swings(pair.process_sample(GetParam().grid_a, GetParam().grid_b)));
}

INSTANTIATE_TEST_SUITE_P(Drives, Small_LongTailedPairSwing, ::testing::ValuesIn(kSwingDrives), drive_name);

class Small_LongTailedPairReset : public ::testing::TestWithParam<Drive>
{
};

// Each row drives one half only, so a reset that skips either half leaves a peak.
TEST_P(Small_LongTailedPairReset, ClearsThePeakGridExcursionOfBothHalves)
{
    SushiDSP::LongTailedPair pair = mark2c_inverter();

    pair.process_sample(GetParam().grid_a, GetParam().grid_b);
    EXPECT_GT(pair.peak_grid_excursion_volts(), 0.0f);

    pair.reset();
    EXPECT_EQ(pair.peak_grid_excursion_volts(), 0.0f);
}

INSTANTIATE_TEST_SUITE_P(Drives, Small_LongTailedPairReset, ::testing::ValuesIn(kResetDrives), drive_name);
