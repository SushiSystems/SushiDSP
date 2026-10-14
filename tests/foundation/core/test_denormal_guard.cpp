/**************************************************************************/
/* test_denormal_guard.cpp                                                */
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
 * @file test_denormal_guard.cpp
 * @brief Tests that DenormalGuard flushes denormals while alive and restores the mode after.
 * @author Mustafa Garip
 */

#include <cmath>
#include <limits>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/core/denormal_guard.hpp>

namespace
{
    // A volatile read stops the compiler folding the product. See tests/foundation/core/README.md.
    float decay_toward_zero(float start)
    {
        volatile float value = start;
        volatile float factor = 0.5f;
        return value * factor;
    }
} // namespace

TEST(Unit_Dsp, DenormalGuardFlushesResultsToZeroWhileAlive)
{
    if (!SushiDSP::DenormalGuard::is_supported())
    {
        GTEST_SKIP() << "no FTZ/DAZ control on this target";
    }
    const float smallest_normal = std::numeric_limits<float>::min();
    {
        SushiDSP::DenormalGuard guard;
        EXPECT_EQ(decay_toward_zero(smallest_normal), 0.0f);
    }
}

TEST(Unit_Dsp, DenormalGuardRestoresPreviousModeOnDestruction)
{
    if (!SushiDSP::DenormalGuard::is_supported())
    {
        GTEST_SKIP() << "no FTZ/DAZ control on this target";
    }
    const float smallest_normal = std::numeric_limits<float>::min();
    const float before = decay_toward_zero(smallest_normal);
    {
        SushiDSP::DenormalGuard guard;
    }
    const float after = decay_toward_zero(smallest_normal);
    EXPECT_EQ(before, after);
    EXPECT_GT(after, 0.0f);
}
