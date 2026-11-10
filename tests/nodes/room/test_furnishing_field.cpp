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
    /// @brief Counts the midpoints per axis of the density integral. A midpoint rule over a bell
    /// wider than 0.15 m (K_MIN_SPREAD_M) is exact up to the boundary terms of Euler-Maclaurin,
    /// h squared over 24 times the slope at the wall, about 1e-3 of the integral at this step.
    constexpr int K_DENSITY_STEPS = 30;

    /// @brief Bounds the miss of the density integral from one, characterised: the Euler-Maclaurin
    /// estimate above is 1e-3 and this is twenty times that.
    constexpr double K_DENSITY_INTEGRAL_TOLERANCE = 0.02;

    /// @brief Counts the random paths of the mean-weight property.
    constexpr int K_RANDOM_PATHS = 20000;

    /// @brief Seeds the path generator, so the property is one fixed sample.
    constexpr std::uint32_t K_PATH_SEED = 20261013u;

    /// @brief Bounds the miss of the length-weighted mean path weight from one, characterised from
    /// the fixed seed: the sampling error of 20000 chords of a four-metre room, with margin.
    constexpr double K_MEAN_WEIGHT_TOLERANCE = 0.15;

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

    /// @brief Returns the integral of the first placed furnishing's density over the room, summed
    /// on a midpoint grid.
    double density_integral(const RoomScene& scene)
    {
        const FurnishingField field(scene);
        const Vec3 size = scene.dimensions;
        double sum = 0.0;
        for (int ix = 0; ix < K_DENSITY_STEPS; ++ix)
        {
            for (int iy = 0; iy < K_DENSITY_STEPS; ++iy)
            {
                for (int iz = 0; iz < K_DENSITY_STEPS; ++iz)
                {
                    sum += field.density(0, Vec3{(ix + 0.5) * size.x / K_DENSITY_STEPS,
                                                 (iy + 0.5) * size.y / K_DENSITY_STEPS,
                                                 (iz + 0.5) * size.z / K_DENSITY_STEPS});
                }
            }
        }
        const double cell = scene.volume_m3() / (static_cast<double>(K_DENSITY_STEPS) * K_DENSITY_STEPS * K_DENSITY_STEPS);
        return sum * cell;
    }

    /// @brief Returns the mean of the path weight over random chords of the unfolded lattice,
    /// weighted by chord length. Chords between uniform points of the lattice sample the room's
    /// volume evenly.
    double mean_path_weight(const RoomScene& scene)
    {
        const FurnishingField field(scene);
        std::mt19937 generator(K_PATH_SEED);
        std::uniform_real_distribution<double> unit(0.0, 1.0);
        const Vec3 size = scene.dimensions;
        double weighted = 0.0;
        double length = 0.0;
        for (int index = 0; index < K_RANDOM_PATHS; ++index)
        {
            const Vec3 from{(unit(generator) * 6.0 - 3.0) * size.x, (unit(generator) * 6.0 - 3.0) * size.y,
                            (unit(generator) * 6.0 - 3.0) * size.z};
            const Vec3 to{unit(generator) * size.x, unit(generator) * size.y, unit(generator) * size.z};
            const double metres = std::sqrt((to.x - from.x) * (to.x - from.x) + (to.y - from.y) * (to.y - from.y) +
                                            (to.z - from.z) * (to.z - from.z));
            weighted += field.path_weight(0, from, to, metres) * metres;
            length += metres;
        }
        return weighted / length;
    }

    /// @brief Names a per-band case after its band index.
    std::string band_name(const ::testing::TestParamInfo<std::size_t>& info)
    {
        return "Band" + std::to_string(info.param);
    }

    const std::vector<std::size_t> kBands = {0, 1, 2, 3, 4, 5};

    // Both paths are real, unreflected lines 3 m long: one at sofa height past it, one far away.
    const Vec3 kNearFrom{0.2, 1.0, 0.4};
    const Vec3 kNearTo{3.2, 1.0, 0.4};
    const Vec3 kFarFrom{2.6, 3.6, 2.0};
    const Vec3 kFarTo{5.6, 3.6, 2.0};
    constexpr double kPathMetres = 3.0;
} // namespace

class Small_FurnishingFieldBands : public ::testing::TestWithParam<std::size_t>
{
};

TEST_P(Small_FurnishingFieldBands, FurnishingFieldAnswersAPathAsTheMediumWhenNothingHasAPlace)
{
    const RoomScene scene = room_with_sofa(false, 0.0, 0.0);
    const FurnishingField field(scene);
    const SushiDSP::room::MediumAbsorption medium(scene);

    const SushiDSP::room::BandResponse by_field = field.over_path(Vec3{-3.0, 7.0, 1.0}, Vec3{4.0, 2.0, 1.2}, 9.0);
    const SushiDSP::room::BandResponse by_medium = medium.over_path(9.0);

    EXPECT_EQ(by_field.gain[GetParam()], by_medium.gain[GetParam()]);
}

TEST_P(Small_FurnishingFieldBands, FurnishingFieldAnswersAModeAsTheMediumWhenNothingHasAPlace)
{
    const RoomScene scene = room_with_sofa(false, 0.0, 0.0);
    const FurnishingField field(scene);
    const SushiDSP::room::MediumAbsorption medium(scene);

    EXPECT_EQ(field.mode_coefficient_per_metre(GetParam(), 1, 0, 0), medium.furnishing_coefficient_per_metre(GetParam()));
}

TEST_P(Small_FurnishingFieldBands, FurnishingFieldLosesMoreOnAPathPastTheFurnishingInEveryBand)
{
    const FurnishingField field(room_with_sofa(true, 1.0, 1.0));

    const SushiDSP::room::BandResponse near_loss = field.over_path(kNearFrom, kNearTo, kPathMetres);
    const SushiDSP::room::BandResponse far_loss = field.over_path(kFarFrom, kFarTo, kPathMetres);

    EXPECT_LT(near_loss.gain[GetParam()], far_loss.gain[GetParam()]);
}

TEST_P(Small_FurnishingFieldBands, FurnishingFieldLeavesTheRoomsDecayTimeAsItWas)
{
    const std::array<double, K_BAND_COUNT> placed = SushiDSP::room::EyringDecay::rt60_seconds(room_with_sofa(true, 1.0, 1.0));
    const std::array<double, K_BAND_COUNT> even = SushiDSP::room::EyringDecay::rt60_seconds(room_with_sofa(false, 0.0, 0.0));

    EXPECT_EQ(placed[GetParam()], even[GetParam()]);
}

INSTANTIATE_TEST_SUITE_P(Bands, Small_FurnishingFieldBands, ::testing::ValuesIn(kBands), band_name);

TEST(Small_FurnishingField, FurnishingFieldReportsNothingPlacedForAnUnplacedFurnishing)
{
    const FurnishingField field(room_with_sofa(false, 0.0, 0.0));

    EXPECT_FALSE(field.placed());
}

TEST(Small_FurnishingField, FurnishingFieldDensityIntegratesToOneForASofaInTheOpen)
{
    EXPECT_NEAR(density_integral(room_with_sofa(true, 3.0, 2.0)), 1.0, K_DENSITY_INTEGRAL_TOLERANCE);
}

TEST(Small_FurnishingField, FurnishingFieldDensityIntegratesToOneForASofaPushedIntoACorner)
{
    // The walls cut the density here, and the scale renormalises it.
    EXPECT_NEAR(density_integral(room_with_sofa(true, 0.2, 0.1)), 1.0, K_DENSITY_INTEGRAL_TOLERANCE);
}

TEST(Small_FurnishingField, FurnishingFieldWeighsAPathPastTheFurnishingAboveThree)
{
    const FurnishingField field(room_with_sofa(true, 1.0, 1.0));

    EXPECT_GT(field.path_weight(0, kNearFrom, kNearTo, kPathMetres), 3.0);
}

TEST(Small_FurnishingField, FurnishingFieldWeighsAPathAwayFromTheFurnishingBelowOneTwentieth)
{
    const FurnishingField field(room_with_sofa(true, 1.0, 1.0));

    EXPECT_LT(field.path_weight(0, kFarFrom, kFarTo, kPathMetres), 0.05);
}

TEST(Medium_FurnishingFieldMonteCarlo, FurnishingFieldWeighsRandomPathsToOneOnAverage)
{
    EXPECT_NEAR(mean_path_weight(room_with_sofa(true, 2.0, 1.5)), 1.0, K_MEAN_WEIGHT_TOLERANCE);
}

TEST(Small_FurnishingField, FurnishingFieldDampsAModeMoreAtItsAntinodeThanInAnEmptyRoom)
{
    // The (1, 0, 0) mode has its pressure antinodes at the x walls.
    EXPECT_LT(decay_of(room_with_sofa(true, 0.3, 2.0), 1, 0, 0), decay_of(room_with_sofa(false, 0.0, 0.0), 1, 0, 0));
}

TEST(Small_FurnishingField, FurnishingFieldDampsAModeLessAtItsNodeThanInAnEmptyRoom)
{
    // The (1, 0, 0) mode has its node at mid-room.
    EXPECT_GT(decay_of(room_with_sofa(true, 3.0, 2.0), 1, 0, 0), decay_of(room_with_sofa(false, 0.0, 0.0), 1, 0, 0));
}

TEST(Small_FurnishingField, FurnishingFieldWeighsAModeAtItsAntinodeAboveOneAndThreeTenthsOfTheMedium)
{
    const RoomScene at_wall = room_with_sofa(true, 0.3, 2.0);
    const FurnishingField field(at_wall);
    const SushiDSP::room::MediumAbsorption medium(at_wall);

    EXPECT_GT(field.mode_coefficient_per_metre(0, 1, 0, 0), 1.3 * medium.furnishing_coefficient_per_metre(0));
}

TEST(Small_FurnishingField, RoomSceneAcceptsAFurnishingPlacedInsideTheRoom)
{
    std::string reason;

    EXPECT_TRUE(room_with_sofa(true, 3.0, 2.0).is_valid(reason)) << reason;
}

TEST(Small_FurnishingField, RoomSceneRefusesAFurnishingPlacedOutsideTheRoomAndNamesWhy)
{
    std::string reason;

    EXPECT_FALSE(room_with_sofa(true, 7.0, 2.0).is_valid(reason));
    EXPECT_NE(reason.find("outside the room"), std::string::npos);
}

TEST(Small_FurnishingField, RoomSceneDoesNotReadThePlaceOfAnUnplacedFurnishing)
{
    std::string reason;

    EXPECT_TRUE(room_with_sofa(false, 7.0, 2.0).is_valid(reason)) << reason;
}
