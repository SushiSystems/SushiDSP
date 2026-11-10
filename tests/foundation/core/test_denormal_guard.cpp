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

/// @brief Skips every test of the suite on a target with no flush-to-zero control.
class Small_DenormalGuard : public ::testing::Test
{
    protected:
        void SetUp() override
        {
            if (!SushiDSP::DenormalGuard::is_supported())
            {
                GTEST_SKIP() << "no FTZ/DAZ control on this target";
            }
        }
};

TEST_F(Small_DenormalGuard, DenormalGuardFlushesResultsToZeroWhileAlive)
{
    const float smallest_normal = std::numeric_limits<float>::min();
    {
        SushiDSP::DenormalGuard guard;
        EXPECT_EQ(decay_toward_zero(smallest_normal), 0.0f);
    }
}

TEST_F(Small_DenormalGuard, DenormalGuardRestoresPreviousModeOnDestruction)
{
    const float smallest_normal = std::numeric_limits<float>::min();
    const float before = decay_toward_zero(smallest_normal);
    {
        SushiDSP::DenormalGuard guard;
    }
    const float after = decay_toward_zero(smallest_normal);
    EXPECT_EQ(before, after);
    EXPECT_GT(after, 0.0f);
}
