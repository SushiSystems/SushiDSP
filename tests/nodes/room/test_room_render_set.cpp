/**************************************************************************/
/* test_room_render_set.cpp                                               */
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
 * @file test_room_render_set.cpp
 * @brief Tests for build_render_set, the join of the four solvers and the HRTF into one record.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include <SushiDSP/nodes/room_scenes/room_fixtures.hpp.hpp>
#include <SushiDSP/foundation/timing_gate/timing_gate.hpp.hpp>
#include <SushiDSP/nodes/room/early_tap_set.hpp>
#include <SushiDSP/nodes/room/eyring_decay.hpp>
#include <SushiDSP/nodes/room/hrtf_set.hpp>
#include <SushiDSP/nodes/room/image_source_solver.hpp>
#include <SushiDSP/nodes/room/room_host.hpp>
#include <SushiDSP/nodes/room/modal_solver.hpp>
#include <SushiDSP/nodes/room/room_render_set.hpp>
#include <SushiDSP/nodes/room/room_scene.hpp>
#include <SushiDSP/nodes/room/spherical_head_hrtf.hpp>

namespace
{
    /// @brief Holds pi.
    constexpr double K_PI = 3.14159265358979323846;

    /// @brief Holds the sample rate every set in this file is built at.
    constexpr double K_SAMPLE_RATE = 48000.0;

    /// @brief Holds the speed of sound the delays are derived with, in metres per second.
    constexpr double K_SPEED_OF_SOUND = 343.0;

    /// @brief Holds the wall-clock budget of one publish: the 30 Hz republish of a slider drag,
    /// 33 ms, the bound spec §4.2 names for the whole scene change.
    constexpr double K_PUBLISH_BUDGET_MS = 33.0;

    /// @brief Returns the spherical-head fallback set, built once at K_SAMPLE_RATE.
    const SushiDSP::room::HrtfSet& fallback_hrtf()
    {
        static const SushiDSP::room::HrtfSet set =
            SushiDSP::room::SphericalHeadHrtf::build(K_SAMPLE_RATE);
        return set;
    }

    /// @brief Returns the distance between two points, in metres.
    double distance_between(const SushiDSP::room::Vec3& a, const SushiDSP::room::Vec3& b)
    {
        const double dx = a.x - b.x;
        const double dy = a.y - b.y;
        const double dz = a.z - b.z;
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    }
} // namespace

TEST(Unit_Dsp, BuildRenderSetFillsEveryLayerForTheGarage)
{
    const SushiDSP::room::RoomScene garage = SushiDSP::test::garage_scene();
    const std::unique_ptr<SushiDSP::room::RoomRenderSet> set = SushiDSP::room::build_render_set(
        garage, fallback_hrtf(), K_SAMPLE_RATE, SushiDSP::room::ListenerKind::Ears);
    ASSERT_NE(set, nullptr);

    // The garage keeps 96 of the 129 order-4 images. See tests/functional/unit/README.md.
    SushiDSP::room::ImageSourceSolver::Options options;
    options.max_order = 4;
    options.listener = SushiDSP::room::ListenerKind::Ears;
    options.sample_rate = K_SAMPLE_RATE;
    const SushiDSP::room::EarlyTapSet solved =
        SushiDSP::room::ImageSourceSolver::solve(garage, options);
    EXPECT_EQ(set->early.count, solved.count);
    EXPECT_GT(set->early.count, 0u);
    EXPECT_LE(set->early.count, SushiDSP::room::EarlyTapSet::K_MAX_EARLY_TAPS);
    EXPECT_FLOAT_EQ(set->early.crossover_time_s, solved.crossover_time_s);

    EXPECT_GT(set->late.line_frames[0], 0u);
    EXPECT_GT(set->late.input_gain, 0.0f);

    // 187 modes lie under the garage's Schroeder frequency, 2000 sqrt(1.1608 / 60) = 278.18 Hz.
    EXPECT_EQ(set->modal.count, 187u);
    EXPECT_NEAR(set->crossover_hz, 278.2, 0.5);
    EXPECT_NEAR(set->crossover_hz, SushiDSP::room::EyringDecay::schroeder_frequency_hz(garage),
                1e-3);
    EXPECT_FLOAT_EQ(set->crossover_hz, set->modal.crossover_hz);
    EXPECT_EQ(set->listener, SushiDSP::room::ListenerKind::Ears);
}

TEST(Unit_Dsp, BuildRenderSetPutsTheDirectPathAtItsPropagationDelay)
{
    // 2.596151 m is 363 frames and a gain of 0.385186. See tests/functional/unit/README.md.
    const SushiDSP::room::RoomScene garage = SushiDSP::test::garage_scene();
    const std::unique_ptr<SushiDSP::room::RoomRenderSet> set = SushiDSP::room::build_render_set(
        garage, fallback_hrtf(), K_SAMPLE_RATE, SushiDSP::room::ListenerKind::Ears);

    const double distance = distance_between(garage.source_position, garage.listener_position);
    EXPECT_NEAR(distance, 2.596151, 1e-6);
    EXPECT_FLOAT_EQ(set->direct_delay_frames, 363.0f);
    EXPECT_FLOAT_EQ(set->direct_delay_frames,
                    static_cast<float>(std::round(distance / K_SPEED_OF_SOUND * K_SAMPLE_RATE)));
    EXPECT_NEAR(set->direct_gain, 0.385186, 1e-5);
    EXPECT_NEAR(set->direct_gain, 1.0 / distance, 1e-6);

    // Straight ahead in the median plane the two ears hear the source at the same time.
    EXPECT_NEAR(set->direct_itd_frames, 0.0, 1e-6);
}

TEST(Unit_Dsp, BuildRenderSetPutsTheMicrophonesDirectPathAtTheMicrophone)
{
    // 0.1 m is 14 frames and a gain of 10. See tests/functional/unit/README.md.
    const SushiDSP::room::RoomScene garage = SushiDSP::test::garage_scene();
    const std::unique_ptr<SushiDSP::room::RoomRenderSet> set = SushiDSP::room::build_render_set(
        garage, fallback_hrtf(), K_SAMPLE_RATE, SushiDSP::room::ListenerKind::Microphone);

    EXPECT_FLOAT_EQ(set->direct_delay_frames, 14.0f);
    EXPECT_NEAR(set->direct_gain, 10.0, 1e-5);
    EXPECT_EQ(set->listener, SushiDSP::room::ListenerKind::Microphone);
    EXPECT_FLOAT_EQ(set->early.taps[0].encode[0], 1.0f);
}

TEST(Unit_Dsp, BuildRenderSetRefusesAnInvalidSceneAndSaysWhy)
{
    SushiDSP::room::RoomScene scene = SushiDSP::test::garage_scene();
    scene.dimensions.x = 15.0;
    try
    {
        SushiDSP::room::build_render_set(scene, fallback_hrtf(), K_SAMPLE_RATE,
                                         SushiDSP::room::ListenerKind::Ears);
        FAIL() << "a 15 m dimension was accepted";
    }
    catch (const std::runtime_error& error)
    {
        const std::string what = error.what();
        EXPECT_NE(what.find("dimension x"), std::string::npos) << what;
        EXPECT_NE(what.find("12.0"), std::string::npos) << what;
    }
}

TEST(Unit_Dsp, BuildRenderSetRefusesAnHrtfSetThatDoesNotFit)
{
    const SushiDSP::room::RoomScene garage = SushiDSP::test::garage_scene();
    EXPECT_THROW(SushiDSP::room::build_render_set(garage, SushiDSP::room::HrtfSet(), K_SAMPLE_RATE,
                                                  SushiDSP::room::ListenerKind::Ears),
                 std::runtime_error);
    EXPECT_THROW(SushiDSP::room::build_render_set(garage, fallback_hrtf(), 44100.0,
                                                  SushiDSP::room::ListenerKind::Ears),
                 std::runtime_error);
}

TEST(Unit_Dsp, BuildRenderSetTakesTheDirectPairFromTheHrtfSet)
{
    // The interaural delay is 0.0006558 s, 31.48 frames. See tests/functional/unit/README.md.
    SushiDSP::room::RoomScene scene = SushiDSP::test::garage_scene();
    scene.listener_position = {4.0, 2.0, 1.2};
    scene.listener_yaw_rad = 0.0;
    scene.source_position = {4.0, 3.0, 1.2};
    const std::unique_ptr<SushiDSP::room::RoomRenderSet> set = SushiDSP::room::build_render_set(
        scene, fallback_hrtf(), K_SAMPLE_RATE, SushiDSP::room::ListenerKind::Ears);

    const double woodworth_frames = SushiDSP::room::SphericalHeadHrtf::K_HEAD_RADIUS_METRES /
                                    K_SPEED_OF_SOUND * (0.5 * K_PI + std::sin(0.5 * K_PI)) *
                                    K_SAMPLE_RATE;
    EXPECT_NEAR(woodworth_frames, 31.479, 1e-3);
    EXPECT_NEAR(set->direct_itd_frames, woodworth_frames, 0.1);

    const SushiDSP::room::HrtfSet::Pair pair = fallback_hrtf().interpolate(0.5 * K_PI, 0.0);
    EXPECT_EQ(set->direct_left, pair.left);
    EXPECT_EQ(set->direct_right, pair.right);
}

TEST(Unit_Dsp, BuildRenderSetCarriesTheDiffuseFieldPair)
{
    SushiDSP::room::HrtfSet hrtf = fallback_hrtf();
    std::array<float, SushiDSP::room::HrtfSet::K_DIFFUSE_TAPS> left{};
    std::array<float, SushiDSP::room::HrtfSet::K_DIFFUSE_TAPS> right{};
    for (std::uint32_t tap = 0; tap < SushiDSP::room::HrtfSet::K_DIFFUSE_TAPS; ++tap)
    {
        left[tap] = static_cast<float>(tap);
        right[tap] = -static_cast<float>(tap);
    }
    hrtf.set_diffuse_field(left, right);

    const std::unique_ptr<SushiDSP::room::RoomRenderSet> set = SushiDSP::room::build_render_set(
        SushiDSP::test::garage_scene(), hrtf, K_SAMPLE_RATE, SushiDSP::room::ListenerKind::Ears);
    EXPECT_EQ(set->diffuse_left, left);
    EXPECT_EQ(set->diffuse_right, right);
}

TEST(Unit_Dsp, BuildRenderSetCostsUnderOnePublishBudget)
{
    SUSHIDSP_REQUIRE_OPTIMISED_BUILD();
    const SushiDSP::room::RoomScene garage = SushiDSP::test::garage_scene();
    const SushiDSP::room::HrtfSet& hrtf = fallback_hrtf();

    const auto start = std::chrono::steady_clock::now();
    const std::unique_ptr<SushiDSP::room::RoomRenderSet> set = SushiDSP::room::build_render_set(
        garage, hrtf, K_SAMPLE_RATE, SushiDSP::room::ListenerKind::Ears);
    const auto stop = std::chrono::steady_clock::now();
    const double milliseconds = std::chrono::duration<double, std::milli>(stop - start).count();
    EXPECT_LT(milliseconds, K_PUBLISH_BUDGET_MS)
        << "taps " << set->early.count << " modes " << set->modal.count;
}

TEST(Unit_Dsp, BuildRenderSetReadsTheStandingWavesAtTheSetsReceiver)
{
    // The modal field is read at the set's receiver. See tests/functional/unit/README.md.
    SushiDSP::room::RoomScene scene = SushiDSP::test::garage_scene();
    scene.source_position = {0.4, 0.5, 0.5};
    scene.listener_position = {5.4, 3.4, 2.0};
    scene.microphone_position = {1.0, 0.5, 0.5};
    const SushiDSP::room::HrtfSet& hrtf = fallback_hrtf();

    std::unique_ptr<SushiDSP::room::RoomRenderSet> microphone = SushiDSP::room::build_render_set(
        scene, hrtf, K_SAMPLE_RATE, SushiDSP::room::ListenerKind::Microphone);
    const std::vector<SushiDSP::room::ModalSolver::Mode> modes =
        SushiDSP::room::ModalSolver::enumerate(scene, microphone->modal.crossover_hz);
    ASSERT_EQ(modes.size(), microphone->modal.count);
    for (std::size_t index = 0; index < modes.size(); ++index)
    {
        const float psi = static_cast<float>(SushiDSP::room::ModalSolver::mode_shape(
            modes[index], scene, scene.microphone_position));
        EXPECT_EQ(microphone->modal.modes[index].ear_gain[0], psi) << "mode " << index;
        EXPECT_EQ(microphone->modal.modes[index].ear_gain[1], psi) << "mode " << index;
    }

    std::unique_ptr<SushiDSP::room::RoomRenderSet> ears = SushiDSP::room::build_render_set(
        scene, hrtf, K_SAMPLE_RATE, SushiDSP::room::ListenerKind::Ears);
    ASSERT_EQ(ears->modal.count, microphone->modal.count);
    const SushiDSP::room::Vec3 left = {5.4, 3.485, 2.0};
    const SushiDSP::room::Vec3 right = {5.4, 3.315, 2.0};
    EXPECT_FLOAT_EQ(
        ears->modal.modes[0].ear_gain[0],
        static_cast<float>(SushiDSP::room::ModalSolver::mode_shape(modes[0], scene, left)));
    EXPECT_FLOAT_EQ(
        ears->modal.modes[0].ear_gain[1],
        static_cast<float>(SushiDSP::room::ModalSolver::mode_shape(modes[0], scene, right)));
    EXPECT_EQ(ears->modal.modes[0].section, microphone->modal.modes[0].section);
}

TEST(Unit_Dsp, RoomRenderSetIsTriviallyCopyable)
{
    static_assert(std::is_trivially_copyable<SushiDSP::room::RoomRenderSet>::value,
                  "it crosses to the audio thread");
    SUCCEED();
}
