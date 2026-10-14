/**************************************************************************/
/* test_furnishing_field.cpp                                              */
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
 * @file test_furnishing_field.cpp
 * @brief Tests where a placed furnishing's absorption lies, for a path and for a mode.
 * @author Mustafa Garip & Sushi Systems
 */

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/nodes/room/band_response.hpp>
#include <SushiDSP/nodes/room/eyring_decay.hpp>
#include <SushiDSP/nodes/room/furnishing_field.hpp>
#include <SushiDSP/nodes/room/furniture.hpp>
#include <SushiDSP/nodes/room/medium_absorption.hpp>
#include <SushiDSP/nodes/room/modal_solver.hpp>
#include <SushiDSP/nodes/room/room_scene.hpp>

using SushiDSP::room::FurnishingField;
using SushiDSP::room::FurnitureId;
using SushiDSP::room::K_BAND_COUNT;
using SushiDSP::room::RoomScene;
using SushiDSP::room::Vec3;

namespace
{
    /// @brief Returns a 6 by 4 by 2.5 m room holding one sofa, placed or not.
    RoomScene room_with_sofa(bool placed, double x, double y)
    {
        RoomScene scene;
        scene.furnishing_count = 1;
        scene.furnishings[0] = RoomScene::Furnishing{FurnitureId::SofaTwoSeat, 1.0, placed, x, y};
        return scene;
    }

    /// @brief Returns the decay of the mode with the given indices, in seconds.
    double decay_of(const RoomScene& scene, std::uint32_t nx, std::uint32_t ny, std::uint32_t nz)
    {
        for (const SushiDSP::room::ModalSolver::Mode& mode :
             SushiDSP::room::ModalSolver::enumerate(scene, 200.0))
        {
            if (mode.nx == nx && mode.ny == ny && mode.nz == nz)
            {
                return mode.decay_seconds;
            }
        }
        ADD_FAILURE() << "the mode was not enumerated";
        return 0.0;
    }
} // namespace

TEST(FurnishingField, FurnishingFieldAnswersAsTheMediumWhenNothingHasAPlace)
{
    const RoomScene scene = room_with_sofa(false, 0.0, 0.0);
    const FurnishingField field(scene);
    const SushiDSP::room::MediumAbsorption medium(scene);

    EXPECT_FALSE(field.placed());
    const SushiDSP::room::BandResponse by_field =
        field.over_path(Vec3{-3.0, 7.0, 1.0}, Vec3{4.0, 2.0, 1.2}, 9.0);
    const SushiDSP::room::BandResponse by_medium = medium.over_path(9.0);
    for (std::size_t band = 0; band < K_BAND_COUNT; ++band)
    {
        EXPECT_EQ(by_field.gain[band], by_medium.gain[band]);
        EXPECT_EQ(field.mode_coefficient_per_metre(band, 1, 0, 0),
                  medium.furnishing_coefficient_per_metre(band));
    }
}

TEST(FurnishingField, FurnishingFieldDensityIntegratesToOneOverTheRoom)
{
    // One sofa in the open and one pushed into a corner, where the walls cut the density.
    for (const RoomScene& scene : {room_with_sofa(true, 3.0, 2.0), room_with_sofa(true, 0.2, 0.1)})
    {
        const FurnishingField field(scene);
        ASSERT_EQ(field.placed_count(), 1u);
        constexpr int steps = 60;
        double sum = 0.0;
        const Vec3 size = scene.dimensions;
        for (int ix = 0; ix < steps; ++ix)
        {
            for (int iy = 0; iy < steps; ++iy)
            {
                for (int iz = 0; iz < steps; ++iz)
                {
                    sum += field.density(0, Vec3{(ix + 0.5) * size.x / steps,
                                                 (iy + 0.5) * size.y / steps,
                                                 (iz + 0.5) * size.z / steps});
                }
            }
        }
        const double cell = scene.volume_m3() / (steps * steps * steps);
        EXPECT_NEAR(sum * cell, 1.0, 0.02);
    }
}

TEST(FurnishingField, FurnishingFieldWeighsAPathPastTheFurnishingAboveOneThatMissesIt)
{
    const RoomScene scene = room_with_sofa(true, 1.0, 1.0);
    const FurnishingField field(scene);

    // Both paths are real, unreflected lines 3 m long at sofa height.
    const double past = field.path_weight(0, Vec3{0.2, 1.0, 0.4}, Vec3{3.2, 1.0, 0.4}, 3.0);
    const double away = field.path_weight(0, Vec3{2.6, 3.6, 2.0}, Vec3{5.6, 3.6, 2.0}, 3.0);
    EXPECT_GT(past, 3.0);
    EXPECT_LT(away, 0.05);

    const SushiDSP::room::BandResponse near_loss =
        field.over_path(Vec3{0.2, 1.0, 0.4}, Vec3{3.2, 1.0, 0.4}, 3.0);
    const SushiDSP::room::BandResponse far_loss =
        field.over_path(Vec3{2.6, 3.6, 2.0}, Vec3{5.6, 3.6, 2.0}, 3.0);
    for (std::size_t band = 0; band < K_BAND_COUNT; ++band)
    {
        EXPECT_LT(near_loss.gain[band], far_loss.gain[band]) << "band " << band;
    }
}

TEST(FurnishingField, FurnishingFieldWeighsRandomPathsToOneOnAverage)
{
    const RoomScene scene = room_with_sofa(true, 2.0, 1.5);
    const FurnishingField field(scene);
    std::mt19937 generator(20261013u);
    std::uniform_real_distribution<double> unit(0.0, 1.0);
    const Vec3 size = scene.dimensions;

    // Chords between uniform points of the unfolded lattice sample the room's volume evenly.
    constexpr int paths = 20000;
    double weighted = 0.0;
    double length = 0.0;
    for (int index = 0; index < paths; ++index)
    {
        const Vec3 from{(unit(generator) * 6.0 - 3.0) * size.x,
                        (unit(generator) * 6.0 - 3.0) * size.y,
                        (unit(generator) * 6.0 - 3.0) * size.z};
        const Vec3 to{unit(generator) * size.x, unit(generator) * size.y,
                      unit(generator) * size.z};
        const double metres = std::sqrt((to.x - from.x) * (to.x - from.x) +
                                        (to.y - from.y) * (to.y - from.y) +
                                        (to.z - from.z) * (to.z - from.z));
        weighted += field.path_weight(0, from, to, metres) * metres;
        length += metres;
    }
    EXPECT_NEAR(weighted / length, 1.0, 0.15);
}

TEST(FurnishingField, FurnishingFieldDampsAModeMoreAtItsAntinodeThanAtItsNode)
{
    // The (1, 0, 0) mode has its pressure antinodes at the x walls and its node at mid-room.
    const RoomScene at_wall = room_with_sofa(true, 0.3, 2.0);
    const RoomScene at_node = room_with_sofa(true, 3.0, 2.0);
    const RoomScene nowhere = room_with_sofa(false, 0.0, 0.0);

    const double wall = decay_of(at_wall, 1, 0, 0);
    const double node = decay_of(at_node, 1, 0, 0);
    const double even = decay_of(nowhere, 1, 0, 0);
    EXPECT_LT(wall, even);
    EXPECT_GT(node, even);

    const FurnishingField field(at_wall);
    const SushiDSP::room::MediumAbsorption medium(at_wall);
    EXPECT_GT(field.mode_coefficient_per_metre(0, 1, 0, 0),
              1.3 * medium.furnishing_coefficient_per_metre(0));
}

TEST(FurnishingField, FurnishingFieldLeavesTheRoomsDecayTimeAsItWas)
{
    const std::array<double, K_BAND_COUNT> placed =
        SushiDSP::room::EyringDecay::rt60_seconds(room_with_sofa(true, 1.0, 1.0));
    const std::array<double, K_BAND_COUNT> even =
        SushiDSP::room::EyringDecay::rt60_seconds(room_with_sofa(false, 0.0, 0.0));
    for (std::size_t band = 0; band < K_BAND_COUNT; ++band)
    {
        EXPECT_EQ(placed[band], even[band]);
    }
}

TEST(FurnishingField, RoomSceneRefusesAFurnishingPlacedOutsideTheRoom)
{
    std::string reason;
    EXPECT_TRUE(room_with_sofa(true, 3.0, 2.0).is_valid(reason)) << reason;
    EXPECT_FALSE(room_with_sofa(true, 7.0, 2.0).is_valid(reason));
    EXPECT_NE(reason.find("outside the room"), std::string::npos);
    // A furnishing with no place is not read for one.
    EXPECT_TRUE(room_with_sofa(false, 7.0, 2.0).is_valid(reason)) << reason;
}
