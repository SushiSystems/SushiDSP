/**************************************************************************/
/* test_source_stage.cpp                                                  */
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
 * @file test_source_stage.cpp
 * @brief Tests that a source stage is idle after reset and keeps its two banks.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/nodes/room/source_stage.hpp>

#include <cstdint>

TEST(Small_SourceStage, IsNotLiveAfterReset)
{
    SushiDSP::Room::SourceStage stage;
    stage.reset();
    EXPECT_FALSE(stage.live());
}

TEST(Small_SourceStage, HasTwoBanks)
{
    EXPECT_EQ(SushiDSP::Room::SourceStage::K_BANKS, 2u);
}
