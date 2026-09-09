/**************************************************************************/
/* test_image_source_solver.cpp                                           */
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
 * @file test_image_source_solver.cpp
 * @brief Tests for ImageSourceSolver, the mirror lattice of the room module.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "RoomFixtures.hpp"
#include "SushiDSP/room/ImageSourceSolver.hpp"

namespace
{
    /// @brief Holds the frames one metre of path takes at 48 kHz and 343 m/s.
    constexpr double kFramesPerMetre = 48000.0 / 343.0;

    /// @brief Returns the spec's section 4.3 geometry: the garage with the source at
    /// (1.5, 2.0, 0.5) and the listener at (4.0, 2.0, 1.2).
    SushiDSP::room::RoomScene example_scene()
    {
        SushiDSP::room::RoomScene scene = SushiDSP::test::garage_scene();
        scene.source_position = { 1.5, 2.0, 0.5 };
        scene.listener_position = { 4.0, 2.0, 1.2 };
        return scene;
    }

    /// @brief Returns the seven first-order path lengths of example_scene(), in metres, in
    /// arrival order: direct, floor, ceiling, -y, +y, -x, +x.
    std::array<double, 7> first_order_metres()
    {
        // dx = 2.5 on every path but the two x images; dz = 0.7 on every path but the floor
        // (1.7) and the ceiling (3.3); dy = 4.0 on the two y images; dx = 5.5 and 6.5 on the
        // two x images.
        return { std::sqrt(2.5 * 2.5 + 0.7 * 0.7), std::sqrt(2.5 * 2.5 + 1.7 * 1.7),
                 std::sqrt(2.5 * 2.5 + 3.3 * 3.3), std::sqrt(2.5 * 2.5 + 4.0 * 4.0 + 0.7 * 0.7),
                 std::sqrt(2.5 * 2.5 + 4.0 * 4.0 + 0.7 * 0.7), std::sqrt(5.5 * 5.5 + 0.7 * 0.7),
                 std::sqrt(6.5 * 6.5 + 0.7 * 0.7) };
    }

    /// @brief Returns the index of the one tap arriving at the given frame within 1e-3, or
    /// the set's count when there is none or more than one.
    std::uint32_t find_tap(const SushiDSP::room::EarlyTapSet& set, double frames)
    {
        std::uint32_t found = set.count;
        std::uint32_t matches = 0;
        for (std::uint32_t tap = 0; tap < set.count; ++tap)
        {
            if (std::fabs(static_cast<double>(set.taps[tap].delay_frames) - frames) < 1e-3)
            {
                found = tap;
                ++matches;
            }
        }
        return matches == 1u ? found : set.count;
    }

    /// @brief Returns an order-4 solve of the example scene with every surface at one
    /// scattering word.
    SushiDSP::room::EarlyTapSet example_with(SushiDSP::room::Scattering scattering)
    {
        SushiDSP::room::RoomScene scene = example_scene();
        scene.scattering.fill(scattering);
        return SushiDSP::room::ImageSourceSolver::solve(scene, {});
    }
}

TEST(Unit_Dsp, ImageSourceSolverCountsTheLatticePoints)
{
    EXPECT_EQ(SushiDSP::room::ImageSourceSolver::image_count(0u), 1u);
    EXPECT_EQ(SushiDSP::room::ImageSourceSolver::image_count(1u), 7u);
    EXPECT_EQ(SushiDSP::room::ImageSourceSolver::image_count(2u), 25u);
    EXPECT_EQ(SushiDSP::room::ImageSourceSolver::image_count(3u), 63u);
    EXPECT_EQ(SushiDSP::room::ImageSourceSolver::image_count(4u), 129u);
}

TEST(Unit_Dsp, ImageSourceSolverPlacesTheFirstOrderArrivalsExactly)
{
    // direct sqrt(6.74) = 2.596151 m, floor sqrt(9.14) = 3.023243 m, ceiling sqrt(17.14) =
    // 4.140048 m, -y and +y sqrt(22.74) = 4.768648 m, -x sqrt(30.74) = 5.544367 m, +x
    // sqrt(42.74) = 6.537584 m; times 48000 / 343 that is 363.3098, 423.0778, 579.3654,
    // 667.3326, 667.3326, 775.8880 and 914.8805 frames. At order 4 t_c is 1749.27 frames and
    // all seven survive; the two y images share one arrival, so the lookup asks for a pair
    // there and a single tap elsewhere.
    const SushiDSP::room::EarlyTapSet set =
        SushiDSP::room::ImageSourceSolver::solve(example_scene(), {});
    const std::array<double, 7> metres = first_order_metres();
    const std::array<double, 7> expected_frames =
        { 363.3098, 423.0778, 579.3654, 667.3326, 667.3326, 775.8880, 914.8805 };
    for (std::size_t tap = 0; tap < 7u; ++tap)
    {
        const double frames = metres[tap] * kFramesPerMetre;
        EXPECT_NEAR(frames, expected_frames[tap], 1e-3) << "tap " << tap;
        std::uint32_t matches = 0;
        for (std::uint32_t index = 0; index < set.count; ++index)
        {
            if (std::fabs(static_cast<double>(set.taps[index].delay_frames) - frames) < 1e-3)
            {
                ++matches;
            }
        }
        EXPECT_EQ(matches, tap == 3u || tap == 4u ? 2u : 1u) << "tap " << tap;
    }
    EXPECT_NEAR(set.taps[0].delay_frames, expected_frames[0], 1e-3);
    EXPECT_NEAR(set.taps[1].delay_frames, expected_frames[1], 1e-3);
}

TEST(Unit_Dsp, ImageSourceSolverCutsTheFirstOrderSetAtItsOwnHandover)
{
    // At order 1 t_c is 2 * 2.5 / 343 = 0.0145773 s, 699.708 frames, so the -x and +x images
    // at 775.888 and 914.881 frames are past it and five taps remain.
    SushiDSP::room::ImageSourceSolver::Options options;
    options.max_order = 1u;
    const SushiDSP::room::EarlyTapSet set =
        SushiDSP::room::ImageSourceSolver::solve(example_scene(), options);
    ASSERT_EQ(set.count, 5u);
    const std::array<double, 7> metres = first_order_metres();
    for (std::size_t tap = 0; tap < 5u; ++tap)
    {
        EXPECT_NEAR(set.taps[tap].delay_frames, metres[tap] * kFramesPerMetre, 1e-3)
            << "tap " << tap;
    }
    EXPECT_NEAR(set.crossover_time_s, 2.0 * 2.5 / 343.0, 1e-6);
}

TEST(Unit_Dsp, ImageSourceSolverSortsItsTapsByArrival)
{
    const std::array<SushiDSP::room::RoomScene, 3> rooms = {
        SushiDSP::test::garage_scene(), SushiDSP::test::bedroom_scene(),
        SushiDSP::test::block_room_scene()
    };
    for (std::size_t room = 0; room < rooms.size(); ++room)
    {
        const SushiDSP::room::EarlyTapSet set =
            SushiDSP::room::ImageSourceSolver::solve(rooms[room], {});
        ASSERT_GT(set.count, 7u) << "room " << room;
        ASSERT_LE(set.count, SushiDSP::room::EarlyTapSet::kMaxEarlyTaps) << "room " << room;
        for (std::uint32_t tap = 1; tap < set.count; ++tap)
        {
            EXPECT_GE(set.taps[tap].delay_frames, set.taps[tap - 1].delay_frames)
                << "room " << room << " tap " << tap;
        }
    }
}

TEST(Unit_Dsp, ImageSourceSolverDropsEveryTapPastTheHandoverTime)
{
    const SushiDSP::room::RoomScene garage = SushiDSP::test::garage_scene();
    // (4 + 1) * 2.5 / 343 = 0.0364431 s, 1749.27 frames at 48 kHz.
    const double crossover = 5.0 * 2.5 / 343.0;
    EXPECT_NEAR(SushiDSP::room::ImageSourceSolver::crossover_time_s(garage, 4u), crossover, 1e-9);
    const SushiDSP::room::EarlyTapSet set = SushiDSP::room::ImageSourceSolver::solve(garage, {});
    EXPECT_NEAR(set.crossover_time_s, crossover, 1e-6);
    for (std::uint32_t tap = 0; tap < set.count; ++tap)
    {
        EXPECT_LE(set.taps[tap].delay_frames, crossover * 48000.0) << "tap " << tap;
    }
    // Order 4 enumerates 129 images and the far corners of the lattice arrive after t_c, so
    // the set is smaller than the lattice.
    EXPECT_LT(set.count, 129u);
}

TEST(Unit_Dsp, ImageSourceSolverFallsOffAsOneOverDistance)
{
    SushiDSP::room::RoomScene scene = example_scene();
    scene.surfaces.fill(SushiDSP::room::MaterialId::OpenDoorway);
    scene.patch_count = 0;
    const SushiDSP::room::EarlyTapSet set = SushiDSP::room::ImageSourceSolver::solve(scene, {});
    ASSERT_EQ(set.count, 1u);
    // 1 / 2.596151 = 0.385186.
    EXPECT_NEAR(set.taps[0].gain, 1.0 / first_order_metres()[0], 1e-5);
    EXPECT_NEAR(set.taps[0].gain, 0.385186, 1e-5);
    EXPECT_NEAR(set.scattered_energy, 0.0f, 1e-12f);
}

TEST(Unit_Dsp, ImageSourceSolverEncodesEarsAndMicrophoneDifferently)
{
    SushiDSP::room::ImageSourceSolver::Options options;
    options.max_order = 2u;
    options.listener = SushiDSP::room::ListenerKind::Ears;
    const SushiDSP::room::EarlyTapSet ears =
        SushiDSP::room::ImageSourceSolver::solve(example_scene(), options);
    ASSERT_GT(ears.count, 1u);
    for (std::uint32_t tap = 0; tap < ears.count; ++tap)
    {
        EXPECT_EQ(ears.taps[tap].encode[0], 1.0f) << "tap " << tap;
        double higher_orders = 0.0;
        for (std::size_t channel = 1; channel < 16u; ++channel)
        {
            higher_orders += std::fabs(static_cast<double>(ears.taps[tap].encode[channel]));
        }
        EXPECT_GT(higher_orders, 0.0) << "tap " << tap;
    }

    options.listener = SushiDSP::room::ListenerKind::Microphone;
    const SushiDSP::room::EarlyTapSet microphone =
        SushiDSP::room::ImageSourceSolver::solve(example_scene(), options);
    ASSERT_GT(microphone.count, 1u);
    for (std::uint32_t tap = 0; tap < microphone.count; ++tap)
    {
        EXPECT_EQ(microphone.taps[tap].encode[0], 1.0f) << "tap " << tap;
        for (std::size_t channel = 1; channel < 16u; ++channel)
        {
            EXPECT_EQ(microphone.taps[tap].encode[channel], 0.0f)
                << "tap " << tap << " channel " << channel;
        }
    }
    // The microphone sits 0.1 m from the source and the ears 2.596 m, so the direct taps differ.
    EXPECT_NEAR(microphone.taps[0].delay_frames, 0.1 * kFramesPerMetre, 1e-3);
    EXPECT_GT(microphone.taps[0].gain, ears.taps[0].gain);
}

TEST(Unit_Dsp, ImageSourceSolverHandsTheScatteredEnergyToTheTail)
{
    const SushiDSP::room::EarlyTapSet flat = example_with(SushiDSP::room::Scattering::Flat);
    const SushiDSP::room::EarlyTapSet cluttered =
        example_with(SushiDSP::room::Scattering::Cluttered);
    ASSERT_EQ(flat.count, cluttered.count);
    const std::array<double, 7> metres = first_order_metres();
    // The direct tap crosses no wall and keeps 1 / r whatever the scattering.
    EXPECT_NEAR(flat.taps[0].gain, 1.0 / metres[0], 1e-6);
    EXPECT_NEAR(cluttered.taps[0].gain, 1.0 / metres[0], 1e-6);
    // One bounce keeps sqrt(1 - s) of the pressure: sqrt(0.90) / r for Flat and sqrt(0.60) / r
    // for Cluttered, so Cluttered is sqrt(0.60 / 0.90) = 0.816497 of Flat. The two y images
    // share an arrival, so the floor, ceiling, -x and +x taps are the ones looked up.
    constexpr std::array<std::size_t, 4> single_arrivals = { 1, 2, 5, 6 };
    for (const std::size_t tap : single_arrivals)
    {
        const std::uint32_t index = find_tap(flat, metres[tap] * kFramesPerMetre);
        ASSERT_LT(index, flat.count) << "tap " << tap;
        EXPECT_NEAR(flat.taps[index].gain, std::sqrt(0.90) / metres[tap], 1e-6) << "tap " << tap;
        EXPECT_NEAR(cluttered.taps[index].gain, std::sqrt(0.60) / metres[tap], 1e-6)
            << "tap " << tap;
        EXPECT_NEAR(cluttered.taps[index].gain / flat.taps[index].gain, 0.816497, 1e-5)
            << "tap " << tap;
    }
    EXPECT_GT(flat.scattered_energy, 0.0f);
    EXPECT_GT(cluttered.scattered_energy, flat.scattered_energy);
}

TEST(Unit_Dsp, ImageSourceSolverSolvesTheGarageUnderTenMilliseconds)
{
    // Spec section 4.2 measured 150 fits at 4.33 ms in a Release build and set the bound its
    // sibling test asserts at 10 ms; a solve is those fits plus the lattice.
    const SushiDSP::room::RoomScene garage = SushiDSP::test::garage_scene();
    const auto start = std::chrono::steady_clock::now();
    const SushiDSP::room::EarlyTapSet set = SushiDSP::room::ImageSourceSolver::solve(garage, {});
    const auto stop = std::chrono::steady_clock::now();
    const double milliseconds = std::chrono::duration<double, std::milli>(stop - start).count();
    EXPECT_LT(milliseconds, 10.0) << "taps " << set.count;
}
