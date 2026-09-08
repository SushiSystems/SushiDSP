/**************************************************************************/
/* test_room_scene.cpp                                                    */
/**************************************************************************/
/*                         This file is part of:                          */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                        https://sushisystems.io                         */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License");        */
/* you may not use this file except in compliance with the License.       */
/* You may obtain a copy of the License at                                */
/*                                                                        */
/*     http://www.apache.org/licenses/LICENSE-2.0                         */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

/**
 * @file test_room_scene.cpp
 * @brief Tests for RoomScene, the shoebox, its materials, its patches and its three poses.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <array>
#include <string>

#include "RoomFixtures.hpp"
#include "SushiDSP/room/RoomScene.hpp"

TEST(Unit_Dsp, RoomSceneComputesTheGaragesVolumeAndArea)
{
    const SushiDSP::room::RoomScene garage = SushiDSP::test::garage_scene();
    EXPECT_NEAR(garage.volume_m3(), 60.0, 1e-9);
    EXPECT_NEAR(garage.surface_area_m2(), 98.0, 1e-9);
}

TEST(Unit_Dsp, RoomSceneWeightsAPatchByItsArea)
{
    // 92 m2 of unpainted concrete and 6 m2 of open doorway over 98 m2:
    // 125 Hz (92 * 0.01 + 6 * 1.0) / 98 = 0.070612; 500 Hz (92 * 0.02 + 6 * 1.0) / 98 = 0.079592.
    const std::array<double, 6> absorption = SushiDSP::test::garage_scene().mean_absorption();
    EXPECT_NEAR(absorption[0], 0.070612, 1e-6);
    EXPECT_NEAR(absorption[2], 0.079592, 1e-6);
}

TEST(Unit_Dsp, RoomSceneComputesTheBedroomsWeightedAbsorption)
{
    // 14 m2 carpet, 51.5 m2 gypsum, 1.5 m2 glass over 67 m2:
    // 500 Hz (14 * 0.14 + 51.5 * 0.05 + 1.5 * 0.18) / 67 = 0.071642.
    EXPECT_NEAR(SushiDSP::test::bedroom_scene().mean_absorption()[2], 0.071642, 1e-6);
}

TEST(Unit_Dsp, RoomSceneRefusesADimensionOutsideItsBounds)
{
    using SushiDSP::room::RoomScene;

    RoomScene too_small = SushiDSP::test::garage_scene();
    too_small.dimensions.x = 1.0;
    std::string reason;
    EXPECT_FALSE(too_small.is_valid(reason));
    EXPECT_NE(reason.find('x'), std::string::npos);
    EXPECT_NE(reason.find("1.5"), std::string::npos);
    EXPECT_NE(reason.find("12"), std::string::npos);

    RoomScene too_large = SushiDSP::test::garage_scene();
    too_large.dimensions.x = 15.0;
    reason.clear();
    EXPECT_FALSE(too_large.is_valid(reason));
    EXPECT_NE(reason.find('x'), std::string::npos);
    EXPECT_NE(reason.find("1.5"), std::string::npos);
    EXPECT_NE(reason.find("12"), std::string::npos);
}

TEST(Unit_Dsp, RoomSceneRefusesAPoseOutsideTheRoom)
{
    using SushiDSP::room::RoomScene;

    RoomScene behind_wall = SushiDSP::test::garage_scene();
    behind_wall.source_position.x = -0.1;
    std::string reason;
    EXPECT_FALSE(behind_wall.is_valid(reason));
    EXPECT_NE(reason.find("source"), std::string::npos);
    EXPECT_NE(reason.find('x'), std::string::npos);

    RoomScene past_wall = SushiDSP::test::garage_scene();
    past_wall.source_position.x = 6.1;
    reason.clear();
    EXPECT_FALSE(past_wall.is_valid(reason));
    EXPECT_NE(reason.find("source"), std::string::npos);
    EXPECT_NE(reason.find('x'), std::string::npos);
}

TEST(Unit_Dsp, RoomSceneRefusesAPatchLargerThanItsFace)
{
    using SushiDSP::room::MaterialId;
    using SushiDSP::room::RoomScene;
    using SushiDSP::room::Surface;

    RoomScene garage = SushiDSP::test::garage_scene();
    garage.patches[0] = { Surface::PlusX, MaterialId::OpenDoorway, 12.0 };
    std::string reason;
    EXPECT_FALSE(garage.is_valid(reason));
    EXPECT_NE(reason.find("12"), std::string::npos);
    EXPECT_NE(reason.find("10"), std::string::npos);
}
