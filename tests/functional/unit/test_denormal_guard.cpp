/****************************************************************/
/* test_denormal_guard.cpp                                      */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file test_denormal_guard.cpp
 * @brief Tests that DenormalGuard flushes denormals while alive and restores the mode after.
 * @author Mustafa Garip
 */

#include <cmath>
#include <limits>

#include <gtest/gtest.h>

#include "SushiDSP/core/DenormalGuard.hpp"

namespace
{
    // A volatile read stops the compiler folding the product. See tests/functional/unit/README.md.
    float decay_toward_zero(float start)
    {
        volatile float value = start;
        volatile float factor = 0.5f;
        return value * factor;
    }
}

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
