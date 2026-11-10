/**************************************************************************/
/* test_steady_block_clock.cpp                                            */
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
 * @file test_steady_block_clock.cpp
 * @brief Tests that the steady BlockClock never reads an earlier time than it read before.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/device/steady_block_clock.hpp>

TEST(Small_SteadyBlockClock, TimeNeverGoesBackwards)
{
    const SushiDSP::SteadyBlockClock clock;

    const std::int64_t first = clock.now_nanoseconds();
    const std::int64_t second = clock.now_nanoseconds();

    EXPECT_GE(second, first);
}
