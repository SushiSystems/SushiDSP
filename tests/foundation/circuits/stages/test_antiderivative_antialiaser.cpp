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
 * @file test_antiderivative_antialiaser.cpp
 * @brief Tests the first-order antiderivative antialiaser: finite output, its bounds and its reset.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/circuits/stages/antiderivative_antialiaser.hpp>
#include <SushiDSP/foundation/circuits/stages/grid_conduction_law.hpp>
#include <SushiDSP/foundation/circuits/stages/plate_swing.hpp>

#include <cmath>

namespace
{
    const SushiDSP::GridConductionLaw K_GRID{1.0e-6f, 0.5f, 0.15f};
    const SushiDSP::PlateSwing K_PLATE{150.0f, 100.0f};

    SushiDSP::AntiderivativeAntialiaser make_stage()
    {
        return SushiDSP::AntiderivativeAntialiaser(1000.0f, 1.0e6f, -2.0f, 20.0f, K_GRID, K_PLATE);
    }
} // namespace

TEST(AntiderivativeAntialiaser, OutputStaysFiniteAndInsideThePlateSwing)
{
    SushiDSP::AntiderivativeAntialiaser stage = make_stage();
    for (int i = -50; i <= 50; ++i)
    {
        const float output = stage.process(0.1f * static_cast<float>(i));
        EXPECT_TRUE(std::isfinite(output));
        EXPECT_LE(output, 150.0f + 1e-3f);
        EXPECT_GE(output, -100.0f - 1e-3f);
    }
}

TEST(AntiderivativeAntialiaser, ResetMakesTheNextSampleAFreshTransfer)
{
    SushiDSP::AntiderivativeAntialiaser fresh = make_stage();
    const float first = fresh.process(1.0f);

    SushiDSP::AntiderivativeAntialiaser used = make_stage();
    used.process(-3.0f);
    used.process(0.5f);
    used.reset();
    EXPECT_FLOAT_EQ(used.process(1.0f), first);
}

TEST(AntiderivativeAntialiaser, RepeatedExcursionGivesTheSameOutput)
{
    SushiDSP::AntiderivativeAntialiaser stage = make_stage();
    const float a = stage.process(0.75f);
    const float b = stage.process(0.75f);
    EXPECT_NEAR(a, b, 1e-3f);
}
