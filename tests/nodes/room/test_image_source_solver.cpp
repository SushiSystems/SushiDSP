/**************************************************************************/
/* test_image_source_solver.cpp                                           */
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
 * @file test_image_source_solver.cpp
 * @brief Tests for ImageSourceSolver, the mirror lattice of the room module.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <SushiDSP/nodes/room_scenes/room_fixtures.hpp>
#include <SushiDSP/foundation/log_capture/log_capture.hpp>
#include <SushiDSP/nodes/room/band_response_fit.hpp>
#include <SushiDSP/nodes/room/image_source_solver.hpp>
#include <SushiDSP/nodes/room/medium_absorption.hpp>
#include <SushiDSP/nodes/room/room_scene.hpp>

namespace
{
    /// @brief Holds the frames one metre of path takes at 48 kHz and 343 m/s.
    constexpr double K_FRAMES_PER_METRE = 48000.0 / 343.0;

    /// @brief Returns the spec's section 4.3 geometry: the garage with the source at
    /// (1.5, 2.0, 0.5) and the listener at (4.0, 2.0, 1.2).
    SushiDSP::room::RoomScene example_scene()
    {
        SushiDSP::room::RoomScene scene = SushiDSP::test::garage_scene();
        scene.source_position = {1.5, 2.0, 0.5};
        scene.listener_position = {4.0, 2.0, 1.2};
        return scene;
    }

    /// @brief Returns the seven first-order path lengths of example_scene(), in metres, in
    /// arrival order: direct, floor, ceiling, -y, +y, -x, +x.
    std::array<double, 7> first_order_metres()
    {
        // These are the seven first-order path lengths. See tests/nodes/room/README.md.
        return {std::sqrt(2.5 * 2.5 + 0.7 * 0.7),
                std::sqrt(2.5 * 2.5 + 1.7 * 1.7),
                std::sqrt(2.5 * 2.5 + 3.3 * 3.3),
                std::sqrt(2.5 * 2.5 + 4.0 * 4.0 + 0.7 * 0.7),
                std::sqrt(2.5 * 2.5 + 4.0 * 4.0 + 0.7 * 0.7),
                std::sqrt(5.5 * 5.5 + 0.7 * 0.7),
                std::sqrt(6.5 * 6.5 + 0.7 * 0.7)};
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

    /// @brief Returns the default scene holding the given bed and sofa counts.
    SushiDSP::room::RoomScene furnished_scene(double beds, double sofas)
    {
        SushiDSP::room::RoomScene scene;
        scene.furnishings[0] = {SushiDSP::room::FurnitureId::BedDouble, beds};
        scene.furnishings[1] = {SushiDSP::room::FurnitureId::SofaTwoSeat, sofas};
        scene.furnishing_count = 2;
        return scene;
    }
} // namespace

TEST(Small_ImageSourceSolver, ImageSourceSolverCountsTheLatticePoints)
{
    EXPECT_EQ(SushiDSP::room::ImageSourceSolver::image_count(0u), 1u);
    EXPECT_EQ(SushiDSP::room::ImageSourceSolver::image_count(1u), 7u);
    EXPECT_EQ(SushiDSP::room::ImageSourceSolver::image_count(2u), 25u);
    EXPECT_EQ(SushiDSP::room::ImageSourceSolver::image_count(3u), 63u);
    EXPECT_EQ(SushiDSP::room::ImageSourceSolver::image_count(4u), 129u);
}

TEST(Small_ImageSourceSolver, ImageSourceSolverPlacesTheFirstOrderArrivalsExactly)
{
    // The two y images share one arrival. See tests/nodes/room/README.md.
    const SushiDSP::room::EarlyTapSet set =
        SushiDSP::room::ImageSourceSolver::solve(example_scene(), {});
    const std::array<double, 7> metres = first_order_metres();
    const std::array<double, 7> expected_frames = {363.3098, 423.0778, 579.3654, 667.3326,
                                                   667.3326, 775.8880, 914.8805};
    for (std::size_t tap = 0; tap < 7u; ++tap)
    {
        const double frames = metres[tap] * K_FRAMES_PER_METRE;
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

TEST(Small_ImageSourceSolver, ImageSourceSolverCutsTheFirstOrderSetAtItsOwnHandover)
{
    // At order 1 t_c is 699.708 frames, so five taps remain. See tests/nodes/room/README.md.
    SushiDSP::room::ImageSourceSolver::Options options;
    options.max_order = 1u;
    const SushiDSP::room::EarlyTapSet set =
        SushiDSP::room::ImageSourceSolver::solve(example_scene(), options);
    ASSERT_EQ(set.count, 5u);
    const std::array<double, 7> metres = first_order_metres();
    for (std::size_t tap = 0; tap < 5u; ++tap)
    {
        EXPECT_NEAR(set.taps[tap].delay_frames, metres[tap] * K_FRAMES_PER_METRE, 1e-3)
            << "tap " << tap;
    }
    EXPECT_NEAR(set.crossover_time_s, 2.0 * 2.5 / 343.0, 1e-6);
}

TEST(Small_ImageSourceSolver, ImageSourceSolverSortsItsTapsByArrival)
{
    const std::array<SushiDSP::room::RoomScene, 3> rooms = {SushiDSP::test::garage_scene(),
                                                            SushiDSP::test::bedroom_scene(),
                                                            SushiDSP::test::block_room_scene()};
    for (std::size_t room = 0; room < rooms.size(); ++room)
    {
        const SushiDSP::room::EarlyTapSet set =
            SushiDSP::room::ImageSourceSolver::solve(rooms[room], {});
        ASSERT_GT(set.count, 7u) << "room " << room;
        ASSERT_LE(set.count, SushiDSP::room::EarlyTapSet::K_MAX_EARLY_TAPS) << "room " << room;
        for (std::uint32_t tap = 1; tap < set.count; ++tap)
        {
            EXPECT_GE(set.taps[tap].delay_frames, set.taps[tap - 1].delay_frames)
                << "room " << room << " tap " << tap;
        }
    }
}

TEST(Small_ImageSourceSolver, ImageSourceSolverDropsEveryTapPastTheHandoverTime)
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
    // Order 4 enumerates 129 images; some arrive after t_c. See tests/nodes/room/README.md.
    EXPECT_LT(set.count, 129u);
}

TEST(Small_ImageSourceSolver, ImageSourceSolverFallsOffAsOneOverDistance)
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

TEST(Small_ImageSourceSolver, ImageSourceSolverEncodesEarsAndMicrophoneDifferently)
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
    // The microphone's line of sight is one tap per element of the 4x12, the nearest first.
    EXPECT_EQ(ears.direct_count, 1u);
    EXPECT_EQ(microphone.direct_count, 28u);
    EXPECT_LT(microphone.taps[0].delay_frames, 0.3 * K_FRAMES_PER_METRE);
    EXPECT_LT(microphone.taps[0].delay_frames, ears.taps[0].delay_frames);
    EXPECT_GT(microphone.taps[0].gain, ears.taps[0].gain);
}

TEST(Small_ImageSourceSolver, ImageSourceSolverHandsTheScatteredEnergyToTheTail)
{
    const SushiDSP::room::EarlyTapSet flat = example_with(SushiDSP::room::Scattering::Flat);
    const SushiDSP::room::EarlyTapSet cluttered =
        example_with(SushiDSP::room::Scattering::Cluttered);
    ASSERT_EQ(flat.count, cluttered.count);
    const std::array<double, 7> metres = first_order_metres();
    // The direct tap crosses no wall and keeps 1 / r whatever the scattering.
    EXPECT_NEAR(flat.taps[0].gain, 1.0 / metres[0], 1e-6);
    EXPECT_NEAR(cluttered.taps[0].gain, 1.0 / metres[0], 1e-6);
    // Cluttered keeps sqrt(0.60 / 0.90) = 0.816497 of Flat. See tests/nodes/room/README.md.
    constexpr std::array<std::size_t, 4> single_arrivals = {1, 2, 5, 6};
    for (const std::size_t tap : single_arrivals)
    {
        const std::uint32_t index = find_tap(flat, metres[tap] * K_FRAMES_PER_METRE);
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

TEST(Small_ImageSourceSolver, ImageSourceSolverSolvesTheGarageToTheSameTapsEveryTimeInsideTheCapacity)
{
    const SushiDSP::room::RoomScene garage = SushiDSP::test::garage_scene();
    const SushiDSP::room::EarlyTapSet first = SushiDSP::room::ImageSourceSolver::solve(garage, {});
    const SushiDSP::room::EarlyTapSet second = SushiDSP::room::ImageSourceSolver::solve(garage, {});

    EXPECT_GT(first.count, 0u);
    EXPECT_LE(first.count, SushiDSP::room::EarlyTapSet::K_MAX_EARLY_TAPS);
    EXPECT_EQ(second.count, first.count);
    EXPECT_FLOAT_EQ(second.crossover_time_s, first.crossover_time_s);
}

TEST(Small_ImageSourceSolver, ImageSourceSolverLeavesTheDirectTapAloneWhenFurnished)
{
    const SushiDSP::room::EarlyTapSet bare =
        SushiDSP::room::ImageSourceSolver::solve(SushiDSP::room::RoomScene{}, {});
    const SushiDSP::room::EarlyTapSet furnished =
        SushiDSP::room::ImageSourceSolver::solve(furnished_scene(3.0, 2.0), {});
    ASSERT_GT(bare.count, 0u);
    ASSERT_GT(furnished.count, 0u);
    EXPECT_EQ(bare.taps[0].section_a, furnished.taps[0].section_a);
    EXPECT_EQ(bare.taps[0].section_b, furnished.taps[0].section_b);
    EXPECT_EQ(bare.taps[0].gain, furnished.taps[0].gain);
    EXPECT_EQ(bare.taps[0].delay_frames, furnished.taps[0].delay_frames);
}

TEST(Small_ImageSourceSolver, ImageSourceSolverKeepsEveryDelayAndGainWhenFurnished)
{
    const SushiDSP::room::EarlyTapSet bare =
        SushiDSP::room::ImageSourceSolver::solve(SushiDSP::room::RoomScene{}, {});
    const SushiDSP::room::EarlyTapSet furnished =
        SushiDSP::room::ImageSourceSolver::solve(furnished_scene(3.0, 2.0), {});
    ASSERT_EQ(bare.count, furnished.count);
    for (std::uint32_t tap = 0; tap < bare.count; ++tap)
    {
        EXPECT_EQ(bare.taps[tap].delay_frames, furnished.taps[tap].delay_frames) << "tap " << tap;
        EXPECT_EQ(bare.taps[tap].gain, furnished.taps[tap].gain) << "tap " << tap;
    }
}

TEST(Small_ImageSourceSolver, ImageSourceSolverThinsAReflectedTapByTheFurnishingLoss)
{
    // Measured worst miss over these taps is 0.68 dB against an expected reduction of up to 3.65 dB.
    constexpr double K_TOLERANCE_DB = 1.0;
    constexpr std::uint32_t K_REFLECTED_TAPS = 12;
    const SushiDSP::room::RoomScene scene = furnished_scene(4.0, 5.0);
    const SushiDSP::room::MediumAbsorption medium(scene);
    const SushiDSP::room::EarlyTapSet bare =
        SushiDSP::room::ImageSourceSolver::solve(SushiDSP::room::RoomScene{}, {});
    const SushiDSP::room::EarlyTapSet furnished =
        SushiDSP::room::ImageSourceSolver::solve(scene, {});
    ASSERT_EQ(bare.count, furnished.count);
    ASSERT_GT(bare.count, K_REFLECTED_TAPS);

    double largest_reduction_db = 0.0;
    double worst_miss_db = 0.0;
    for (std::uint32_t tap = 1; tap <= K_REFLECTED_TAPS; ++tap)
    {
        const double metres = static_cast<double>(bare.taps[tap].delay_frames) / K_FRAMES_PER_METRE;
        for (std::size_t band = 0; band < SushiDSP::room::K_BAND_COUNT; ++band)
        {
            const double centre = SushiDSP::room::K_BAND_CENTRES_HZ[band];
            const double measured =
                SushiDSP::room::BandResponseFit::magnitude_db(
                    {furnished.taps[tap].section_a, furnished.taps[tap].section_b}, centre,
                    48000.0) -
                SushiDSP::room::BandResponseFit::magnitude_db(
                    {bare.taps[tap].section_a, bare.taps[tap].section_b}, centre, 48000.0);
            const double expected =
                10.0 *
                std::log10(std::exp(-medium.furnishing_coefficient_per_metre(band) * metres));
            largest_reduction_db = std::max(largest_reduction_db, -expected);
            worst_miss_db = std::max(worst_miss_db, std::fabs(measured - expected));
            EXPECT_NEAR(measured, expected, K_TOLERANCE_DB) << "tap " << tap << " band " << band;
        }
    }
    EXPECT_GT(largest_reduction_db, 3.0 * K_TOLERANCE_DB);
    EXPECT_LT(worst_miss_db, K_TOLERANCE_DB);
}

TEST(Small_ImageSourceSolverLogging, SolveWarnsOnceWhenTheTapsAreCutAtTheCapacity)
{
    SushiDSP::room::ImageSourceSolver::Options options;
    options.max_order = 8u;

    SushiDSP::LogCapture capture;
    const SushiDSP::room::EarlyTapSet set =
        SushiDSP::room::ImageSourceSolver::solve(SushiDSP::test::garage_scene(), options);

    EXPECT_EQ(set.count, SushiDSP::room::EarlyTapSet::K_MAX_EARLY_TAPS);
    const std::vector<SushiDSP::CapturedLogLine> lines = capture.lines();
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0].level, SushiLog::LogLevel::Warn);
    EXPECT_EQ(lines[0].category, "room");
    EXPECT_NE(lines[0].message.find("kept 128 early taps"), std::string::npos) << lines[0].message;
}

TEST(Small_ImageSourceSolverLogging, SolveWritesNothingWhenNoTapIsLeftOut)
{
    SushiDSP::room::ImageSourceSolver::Options options;
    options.max_order = 1u;

    SushiDSP::LogCapture capture;
    const SushiDSP::room::EarlyTapSet set =
        SushiDSP::room::ImageSourceSolver::solve(example_scene(), options);

    EXPECT_EQ(set.count, 5u);
    EXPECT_TRUE(capture.lines().empty());
}
