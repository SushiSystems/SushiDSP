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
 * @file test_long_tailed_pair.cpp
 * @brief Checks the pair's split, its rejection of a common-mode move and its plate bounds.
 * @author Mustafa Garip & Sushi Systems
 */

#include <cmath>

#include <gtest/gtest.h>

#include "SushiDSP/circuits/stages/LongTailedPair.hpp"

namespace
{
    /// @brief The Mark IIC+'s inverter, as MesaMark2c.cpp builds it.
    /// @return A pair on S8's 82.5 k and 90.9 k plates over the feedback leg's 27.27 k tail.
    SushiDSP::LongTailedPair mark2c_inverter()
    {
        return SushiDSP::LongTailedPair(
            SushiDSP::LongTailedPairTube{ 96.378299f, 34061.086f, -1.5f,
                                          SushiDSP::kTriode12AX7GridConduction },
            SushiDSP::LongTailedPairHalf{ 82500.0f, 25000.0f, 100000.0f,
                                          SushiDSP::PlateSwing{ 106.646f, 144.645f } },
            SushiDSP::LongTailedPairHalf{ 90900.0f, 2869.2f, 150000.0f,
                                          SushiDSP::PlateSwing{ 117.503f, 159.365f } },
            26800.0f, 470.0f);
    }
}

TEST(Unit_Dsp, LongTailedPairSplitsASingleEndedInputNearlyInHalf)
{
    SushiDSP::LongTailedPair pair = mark2c_inverter();

    // gm Rt / (1 + 2 gm Rt), gm 2.8296 mS into the 27.27 k the feedback leg returns through.
    EXPECT_NEAR(pair.cathode_coupling(), 0.49678, 1e-4);
    EXPECT_LT(pair.cathode_coupling(), 0.5);
}

TEST(Unit_Dsp, LongTailedPairDrivesItsTwoPlatesInOppositeDirections)
{
    SushiDSP::LongTailedPair pair = mark2c_inverter();

    const SushiDSP::LongTailedPair::Plates plates = pair.process_sample(0.05f, 0.0f);

    EXPECT_LT(plates.a, 0.0f);
    EXPECT_GT(plates.b, 0.0f);
    // Matched amplitudes are not asserted. See tests/functional/unit/README.md.
    EXPECT_GT(std::fabs(plates.a), 0.0f);
    EXPECT_GT(std::fabs(plates.b), 0.0f);
}

TEST(Unit_Dsp, LongTailedPairRejectsACommonModeMove)
{
    SushiDSP::LongTailedPair pair = mark2c_inverter();

    const SushiDSP::LongTailedPair::Plates plates = pair.process_sample(0.05f, 0.05f);

    // Both grids moving together lift the shared cathode with them, so neither gap sees much.
    EXPECT_NEAR(static_cast<double>(plates.a - plates.b), 0.0, 0.05);
}

TEST(Unit_Dsp, LongTailedPairBoundsBothPlatesByTheirOwnSwings)
{
    SushiDSP::LongTailedPair pair = mark2c_inverter();

    for (int i = 0; i < 64; ++i)
    {
        const SushiDSP::LongTailedPair::Plates plates = pair.process_sample(400.0f, -400.0f);
        EXPECT_TRUE(std::isfinite(plates.a));
        EXPECT_TRUE(std::isfinite(plates.b));
        EXPECT_LE(plates.a, 106.646f);
        EXPECT_GE(plates.a, -144.645f);
        EXPECT_LE(plates.b, 117.503f);
        EXPECT_GE(plates.b, -159.365f);
    }
}

TEST(Unit_Dsp, LongTailedPairResetClearsBothHalves)
{
    SushiDSP::LongTailedPair pair = mark2c_inverter();

    pair.process_sample(200.0f, 0.0f);
    EXPECT_GT(pair.peak_grid_excursion_volts(), 0.0f);

    pair.reset();
    EXPECT_FLOAT_EQ(pair.peak_grid_excursion_volts(), 0.0f);
}
