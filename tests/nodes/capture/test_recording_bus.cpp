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

#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/nodes/capture/recording_bus.hpp>

TEST(Small_RecordingBus, RecordingBusRefusesABlockBeforeItIsArmed)
{
    SushiDSP::RecordingBus bus(1u, 48000.0);
    const std::vector<float> processed(128u, 0.4f);
    const std::vector<float> dry(128u, -0.2f);
    const float* processed_channels[1] = {processed.data()};

    EXPECT_FALSE(bus.write(processed_channels, dry.data(), 128u));
}
