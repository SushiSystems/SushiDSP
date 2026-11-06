/**************************************************************************/
/* test_recording_bus.cpp                                                 */
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
 * @file test_recording_bus.cpp
 * @brief Tests that a recording bus starts idle and healthy and holds the ring size it documents.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/nodes/capture/recording_bus.hpp>

TEST(Small_RecordingBus, RingHoldsSixtyFiveThousandFramesByDefault)
{
    EXPECT_EQ(SushiDSP::RecordingBus::K_RING_CAPACITY_FRAMES, 65536u);
}

TEST(Small_RecordingBus, StartsIdleAndWithoutFailure)
{
    const SushiDSP::RecordingBus bus(2u, 48000.0);
    EXPECT_FALSE(bus.is_recording());
    EXPECT_FALSE(bus.failed());
}
