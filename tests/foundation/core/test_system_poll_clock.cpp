/**************************************************************************/
/* test_system_poll_clock.cpp                                             */
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
 * @file test_system_poll_clock.cpp
 * @brief Tests that the system PollClock returns at once after an interrupt instead of sleeping its period.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/core/system_poll_clock.hpp>

#include <chrono>

TEST(Small_SystemPollClock, AnInterruptedWaitReturnsInLessThanAPeriod)
{
    constexpr std::chrono::milliseconds K_PERIOD{500};
    SushiDSP::SystemPollClock clock;
    clock.interrupt();

    const auto start = std::chrono::steady_clock::now();
    clock.wait_for_next_poll();
    const auto elapsed = std::chrono::steady_clock::now() - start;

    EXPECT_LT(elapsed, K_PERIOD);
}
