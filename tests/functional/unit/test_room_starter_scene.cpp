/**************************************************************************/
/* test_room_starter_scene.cpp                                            */
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
 * @file test_room_starter_scene.cpp
 * @brief Tests for the starter scene a fresh room slot loads: its shape, its wiring into a fresh
 * RoomNode and the decay it gives.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include "SushiDSP/room/band_response.hpp"
#include "SushiDSP/room/eyring_decay.hpp"
#include "SushiDSP/room/furniture.hpp"
#include "SushiDSP/room/materials.hpp"
#include "SushiDSP/room/room_node.hpp"
#include "SushiDSP/room/room_scene.hpp"
#include "SushiDSP/room/starter_scene.hpp"

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

    /// @brief Names the 1 kHz octave band's index.
    constexpr std::size_t K_BAND1K_HZ = 3;

    /// @brief Bounds the starter scene's reverberation time from above, in seconds.
    constexpr double K_UPPER_BOUND_SECONDS = 1.3;

    /// @brief Bounds the starter scene's reverberation time from below, in seconds.
    constexpr double K_LOWER_BOUND_SECONDS = 0.5;
} // namespace

TEST(Unit_Dsp, StarterSceneIsValid)
{
    std::string reason;
    EXPECT_TRUE(starter_scene().is_valid(reason)) << reason;
}

TEST(Unit_Dsp, StarterSceneIsTheTokiRoomOfTheDesign)
{
    const RoomScene scene = starter_scene();

    EXPECT_EQ(scene.dimensions.x, 6.0);
    EXPECT_EQ(scene.dimensions.y, 4.0);
    EXPECT_EQ(scene.dimensions.z, 2.6);
    EXPECT_EQ(scene.surfaces[static_cast<std::size_t>(Surface::Floor)],
              MaterialId::ParquetOnConcrete);
    for (const Surface face :
         {Surface::MinusX, Surface::PlusX, Surface::MinusY, Surface::PlusY, Surface::Ceiling})
    {
        EXPECT_EQ(scene.surfaces[static_cast<std::size_t>(face)],
                  MaterialId::PlasterOnMasonryPainted)
            << "face " << static_cast<std::size_t>(face);
    }
}

TEST(Unit_Dsp, StarterSceneCarriesACurtainARugAndADoor)
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

TEST(Unit_Dsp, StarterSceneCarriesABedADeskAWardrobeAndAChairButNoSofa)
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

    for (std::uint32_t index = 0; index < scene.furnishing_count; ++index)
    {
        EXPECT_NE(scene.furnishings[index].item, FurnitureId::SofaTwoSeat) << "entry " << index;
    }
}

TEST(Unit_Dsp, StarterSceneKeepsTheDefaultPosesAndScattering)
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

TEST(Unit_Dsp, RoomNodeStartsFromTheStarterScene)
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

TEST(Unit_Dsp, StarterSceneDecaysInsideTheRecordedBandAndShorterThanTheBareDefault)
{
    const std::array<double, K_BAND_COUNT> starter = EyringDecay::rt60_seconds(starter_scene());
    const std::array<double, K_BAND_COUNT> bare = EyringDecay::rt60_seconds(RoomScene{});

    for (const std::size_t band : {K_BAND500_HZ, K_BAND1K_HZ})
    {
        EXPECT_LT(starter[band], K_UPPER_BOUND_SECONDS) << "band " << band;
        EXPECT_GT(starter[band], K_LOWER_BOUND_SECONDS) << "band " << band;
    }
    EXPECT_LT(starter[K_BAND500_HZ], bare[K_BAND500_HZ]);
}

TEST(Unit_Dsp, StarterSceneWithoutItsFurnishingsDecaysLongerInEveryBand)
{
    const RoomScene furnished = starter_scene();
    RoomScene empty = furnished;
    empty.furnishing_count = 0;

    const std::array<double, K_BAND_COUNT> with = EyringDecay::rt60_seconds(furnished);
    const std::array<double, K_BAND_COUNT> without = EyringDecay::rt60_seconds(empty);
    for (std::size_t band = 0; band < K_BAND_COUNT; ++band)
    {
        EXPECT_GT(without[band], with[band]) << "band " << band;
    }
}

TEST(Unit_Dsp, StarterSceneCarriesTheBuiltRoomLossAndTheBareSceneDoesNot)
{
    EXPECT_TRUE(starter_scene().built_room_loss);
    EXPECT_FALSE(RoomScene{}.built_room_loss);
}
