/**************************************************************************/
/* test_starter_scene.cpp                                                 */
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
 * @file test_starter_scene.cpp
 * @brief Tests for the starter scene a fresh room slot loads: its shape, its wiring into a fresh
 * RoomNode and the decay it gives.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include <SushiDSP/nodes/room/band_response.hpp>
#include <SushiDSP/nodes/room/eyring_decay.hpp>
#include <SushiDSP/nodes/room/furniture.hpp>
#include <SushiDSP/nodes/room/materials.hpp>
#include <SushiDSP/nodes/room/room_node.hpp>
#include <SushiDSP/nodes/room/room_scene.hpp>
#include <SushiDSP/nodes/room/starter_scene.hpp>

namespace
{
    using SushiDSP::room::EyringDecay;
    using SushiDSP::room::FurnitureId;
    using SushiDSP::room::K_BAND_COUNT;
    using SushiDSP::room::MaterialId;
    using SushiDSP::room::RoomScene;
    using SushiDSP::room::starter_scene;
    using SushiDSP::room::Surface;

    /// @brief Names the 500 Hz octave band's index.
    constexpr std::size_t K_BAND500_HZ = 2;

    /// @brief Bounds the starter scene's reverberation time from above, in seconds.
    constexpr double K_UPPER_BOUND_SECONDS = 1.3;

    /// @brief Bounds the starter scene's reverberation time from below, in seconds.
    constexpr double K_LOWER_BOUND_SECONDS = 0.5;

    /// @brief Returns whether a scene holds an item among its live furnishings.
    bool holds_item(const RoomScene& scene, FurnitureId item)
    {
        return std::any_of(scene.furnishings.begin(),
                           scene.furnishings.begin() + scene.furnishing_count,
                           [item](const RoomScene::Furnishing& entry) { return entry.item == item; });
    }

    /// @brief Holds one face of the starter room and the material the design gives it.
    struct Face
    {
            const char* name;
            Surface surface;
            MaterialId material;
    };

    /// @brief Holds the six faces: parquet on concrete underfoot, painted plaster everywhere else.
    const std::array<Face, 6> kFaces = {
        {{"Floor", Surface::Floor, MaterialId::ParquetOnConcrete},
         {"MinusX", Surface::MinusX, MaterialId::PlasterOnMasonryPainted},
         {"PlusX", Surface::PlusX, MaterialId::PlasterOnMasonryPainted},
         {"MinusY", Surface::MinusY, MaterialId::PlasterOnMasonryPainted},
         {"PlusY", Surface::PlusY, MaterialId::PlasterOnMasonryPainted},
         {"Ceiling", Surface::Ceiling, MaterialId::PlasterOnMasonryPainted}}};

    /// @brief Names a face case after its face.
    std::string face_name(const ::testing::TestParamInfo<Face>& info)
    {
        return info.param.name;
    }

    /// @brief Holds one octave band the decay checks read.
    struct Band
    {
            const char* name;
            std::size_t index;
    };

    /// @brief Holds the six octave bands, 125 Hz to 4 kHz.
    const std::array<Band, K_BAND_COUNT> kBands = {{{"Hz125", 0},
                                                    {"Hz250", 1},
                                                    {"Hz500", 2},
                                                    {"Hz1000", 3},
                                                    {"Hz2000", 4},
                                                    {"Hz4000", 5}}};

    /// @brief Holds the two bands whose decay the design records: 500 Hz and 1 kHz.
    const std::array<Band, 2> kRecordedBands = {{{"Hz500", 2}, {"Hz1000", 3}}};

    /// @brief Names a band case after its centre.
    std::string band_name(const ::testing::TestParamInfo<Band>& info)
    {
        return info.param.name;
    }
} // namespace

class Small_StarterSceneFaces : public ::testing::TestWithParam<Face>
{
};

class Small_StarterSceneBands : public ::testing::TestWithParam<Band>
{
};

class Small_StarterSceneRecordedBands : public ::testing::TestWithParam<Band>
{
};

TEST(Small_StarterScene, StarterSceneIsValid)
{
    std::string reason;
    EXPECT_TRUE(starter_scene().is_valid(reason)) << reason;
}

TEST(Small_StarterScene, StarterRoomIsSixByFourByTwoPointSixMetres)
{
    const RoomScene scene = starter_scene();

    EXPECT_EQ(scene.dimensions.x, 6.0);
    EXPECT_EQ(scene.dimensions.y, 4.0);
    EXPECT_EQ(scene.dimensions.z, 2.6);
}

TEST_P(Small_StarterSceneFaces, FaceCarriesTheMaterialOfTheDesign)
{
    EXPECT_EQ(starter_scene().surfaces[static_cast<std::size_t>(GetParam().surface)],
              GetParam().material);
}

INSTANTIATE_TEST_SUITE_P(Faces, Small_StarterSceneFaces, ::testing::ValuesIn(kFaces), face_name);

TEST(Small_StarterScene, StarterSceneCarriesACurtainARugAndADoor)
{
    const RoomScene scene = starter_scene();
    ASSERT_EQ(scene.patch_count, 3u);

    EXPECT_EQ(scene.patches[0].surface, Surface::PlusY);
    EXPECT_EQ(scene.patches[0].material, MaterialId::CurtainHeavyVelour);
    EXPECT_EQ(scene.patches[0].area_m2, 4.0);

    EXPECT_EQ(scene.patches[1].surface, Surface::Floor);
    EXPECT_EQ(scene.patches[1].material, MaterialId::ThinCarpetOnConcrete);
    EXPECT_EQ(scene.patches[1].area_m2, 12.0);

    EXPECT_EQ(scene.patches[2].surface, Surface::MinusX);
    EXPECT_EQ(scene.patches[2].material, MaterialId::WoodenDoor);
    EXPECT_EQ(scene.patches[2].area_m2, 1.9);
}

TEST(Small_StarterScene, StarterSceneCarriesABedADeskAWardrobeAndAChair)
{
    const RoomScene scene = starter_scene();
    ASSERT_EQ(scene.furnishing_count, 4u);

    EXPECT_EQ(scene.furnishings[0].item, FurnitureId::BedDouble);
    EXPECT_EQ(scene.furnishings[0].quantity, 1.0);
    EXPECT_EQ(scene.furnishings[1].item, FurnitureId::Desk);
    EXPECT_EQ(scene.furnishings[1].quantity, 1.0);
    EXPECT_EQ(scene.furnishings[2].item, FurnitureId::WardrobeModule);
    EXPECT_EQ(scene.furnishings[2].quantity, 3.0);
    EXPECT_EQ(scene.furnishings[3].item, FurnitureId::ChairUpholstered);
    EXPECT_EQ(scene.furnishings[3].quantity, 1.0);
}

TEST(Small_StarterScene, StarterSceneCarriesNoSofa)
{
    EXPECT_FALSE(holds_item(starter_scene(), FurnitureId::SofaTwoSeat));
}

TEST(Small_StarterScene, StarterSceneKeepsTheDefaultPosesAndScattering)
{
    const RoomScene starter = starter_scene();
    const RoomScene bare;

    EXPECT_EQ(starter.scattering, bare.scattering);
    EXPECT_EQ(starter.source_position.x, bare.source_position.x);
    EXPECT_EQ(starter.source_position.y, bare.source_position.y);
    EXPECT_EQ(starter.source_position.z, bare.source_position.z);
    EXPECT_EQ(starter.listener_position.x, bare.listener_position.x);
    EXPECT_EQ(starter.listener_position.y, bare.listener_position.y);
    EXPECT_EQ(starter.listener_position.z, bare.listener_position.z);
    EXPECT_EQ(starter.microphone_position.x, bare.microphone_position.x);
    EXPECT_EQ(starter.microphone_position.y, bare.microphone_position.y);
    EXPECT_EQ(starter.microphone_position.z, bare.microphone_position.z);
    EXPECT_EQ(starter.microphone_pattern, bare.microphone_pattern);
}

TEST(Small_StarterScene, RoomNodeStartsFromTheStarterScene)
{
    SushiDSP::room::RoomNode node;
    const RoomScene starter = starter_scene();
    const RoomScene held = node.scene();

    EXPECT_EQ(held.dimensions.x, starter.dimensions.x);
    EXPECT_EQ(held.dimensions.y, starter.dimensions.y);
    EXPECT_EQ(held.dimensions.z, starter.dimensions.z);
    EXPECT_EQ(held.patch_count, starter.patch_count);
    EXPECT_EQ(held.furnishing_count, starter.furnishing_count);
    EXPECT_DOUBLE_EQ(node.schroeder_frequency_hz(), EyringDecay::schroeder_frequency_hz(starter));
}

TEST_P(Small_StarterSceneRecordedBands, StarterSceneDecaysUnderTheUpperBound)
{
    EXPECT_LT(EyringDecay::rt60_seconds(starter_scene())[GetParam().index], K_UPPER_BOUND_SECONDS);
}

TEST_P(Small_StarterSceneRecordedBands, StarterSceneDecaysOverTheLowerBound)
{
    EXPECT_GT(EyringDecay::rt60_seconds(starter_scene())[GetParam().index], K_LOWER_BOUND_SECONDS);
}

INSTANTIATE_TEST_SUITE_P(Bands, Small_StarterSceneRecordedBands, ::testing::ValuesIn(kRecordedBands),
                         band_name);

TEST(Small_StarterScene, StarterSceneDecaysShorterThanTheBareDefaultAtFiveHundredHertz)
{
    EXPECT_LT(EyringDecay::rt60_seconds(starter_scene())[K_BAND500_HZ],
              EyringDecay::rt60_seconds(RoomScene{})[K_BAND500_HZ]);
}

TEST_P(Small_StarterSceneBands, StarterSceneWithoutItsFurnishingsDecaysLonger)
{
    RoomScene empty = starter_scene();
    empty.furnishing_count = 0;

    EXPECT_GT(EyringDecay::rt60_seconds(empty)[GetParam().index],
              EyringDecay::rt60_seconds(starter_scene())[GetParam().index]);
}

TEST_P(Small_StarterSceneBands, StarterSceneWithoutItsBuiltRoomLossDecaysLonger)
{
    RoomScene without = starter_scene();
    without.built_room_loss = false;

    EXPECT_GT(EyringDecay::rt60_seconds(without)[GetParam().index],
              EyringDecay::rt60_seconds(starter_scene())[GetParam().index]);
}

INSTANTIATE_TEST_SUITE_P(Bands, Small_StarterSceneBands, ::testing::ValuesIn(kBands), band_name);

TEST(Small_StarterScene, StarterSceneCarriesTheBuiltRoomLoss)
{
    EXPECT_TRUE(starter_scene().built_room_loss);
}
