/**************************************************************************/
/* test_room_render_set.cpp                                               */
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

#include "RoomFixtures.hpp"
#include "SushiDSP/room/EarlyTapSet.hpp"
#include "SushiDSP/room/EyringDecay.hpp"
#include "SushiDSP/room/HrtfSet.hpp"
#include "SushiDSP/room/ImageSourceSolver.hpp"
#include "SushiDSP/room/IRoomHost.hpp"
#include "SushiDSP/room/RoomRenderSet.hpp"
#include "SushiDSP/room/RoomScene.hpp"
#include "SushiDSP/room/SphericalHeadHrtf.hpp"

namespace
{
    /// @brief Holds pi.
    constexpr double kPi = 3.14159265358979323846;

    /// @brief Holds the sample rate every set in this file is built at.
    constexpr double kSampleRate = 48000.0;

    /// @brief Holds the speed of sound the delays are derived with, in metres per second.
    constexpr double kSpeedOfSound = 343.0;

    /// @brief Holds the wall-clock budget of one publish: the 30 Hz republish of a slider drag,
    /// 33 ms, the bound spec §4.2 names for the whole scene change.
    constexpr double kPublishBudgetMs = 33.0;

    /// @brief Returns the spherical-head fallback set, built once at kSampleRate.
    const SushiDSP::room::HrtfSet& fallback_hrtf()
    {
        static const SushiDSP::room::HrtfSet set =
            SushiDSP::room::SphericalHeadHrtf::build(kSampleRate);
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
}

TEST(Unit_Dsp, BuildRenderSetFillsEveryLayerForTheGarage)
{
    const SushiDSP::room::RoomScene garage = SushiDSP::test::garage_scene();
    const std::unique_ptr<SushiDSP::room::RoomRenderSet> set = SushiDSP::room::build_render_set(
        garage, fallback_hrtf(), kSampleRate, SushiDSP::room::ListenerKind::Ears);
    ASSERT_NE(set, nullptr);

    // The early set is the solver's own at order 4: 129 images, of which the garage keeps the
    // 96 that arrive before t_c = 5 * 2.5 / 343 s, the 12.5 m the shortest dimension allows.
    SushiDSP::room::ImageSourceSolver::Options options;
    options.max_order = 4;
    options.listener = SushiDSP::room::ListenerKind::Ears;
    options.sample_rate = kSampleRate;
    const SushiDSP::room::EarlyTapSet solved =
        SushiDSP::room::ImageSourceSolver::solve(garage, options);
    EXPECT_EQ(set->early.count, solved.count);
    EXPECT_GT(set->early.count, 0u);
    EXPECT_LE(set->early.count, SushiDSP::room::EarlyTapSet::kMaxEarlyTaps);
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
    // The source (1.5, 2.0, 0.5) and the listener (4.0, 2.0, 1.2) are sqrt(2.5^2 + 0.7^2) =
    // sqrt(6.74) = 2.596151 m apart: round(2.596151 / 343 * 48000) = round(363.31) = 363 frames
    // and 1 / 2.596151 = 0.385186.
    const SushiDSP::room::RoomScene garage = SushiDSP::test::garage_scene();
    const std::unique_ptr<SushiDSP::room::RoomRenderSet> set = SushiDSP::room::build_render_set(
        garage, fallback_hrtf(), kSampleRate, SushiDSP::room::ListenerKind::Ears);

    const double distance = distance_between(garage.source_position, garage.listener_position);
    EXPECT_NEAR(distance, 2.596151, 1e-6);
    EXPECT_FLOAT_EQ(set->direct_delay_frames, 363.0f);
    EXPECT_FLOAT_EQ(set->direct_delay_frames,
                    static_cast<float>(std::round(distance / kSpeedOfSound * kSampleRate)));
    EXPECT_NEAR(set->direct_gain, 0.385186, 1e-5);
    EXPECT_NEAR(set->direct_gain, 1.0 / distance, 1e-6);

    // Straight ahead in the median plane the two ears hear the source at the same time.
    EXPECT_NEAR(set->direct_itd_frames, 0.0, 1e-6);
}

TEST(Unit_Dsp, BuildRenderSetPutsTheMicrophonesDirectPathAtTheMicrophone)
{
    // The microphone sits 0.1 m from the source: round(0.1 / 343 * 48000) = round(13.99) = 14
    // frames and a gain of 10.
    const SushiDSP::room::RoomScene garage = SushiDSP::test::garage_scene();
    const std::unique_ptr<SushiDSP::room::RoomRenderSet> set = SushiDSP::room::build_render_set(
        garage, fallback_hrtf(), kSampleRate, SushiDSP::room::ListenerKind::Microphone);

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
        SushiDSP::room::build_render_set(scene, fallback_hrtf(), kSampleRate,
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
    EXPECT_THROW(SushiDSP::room::build_render_set(garage, SushiDSP::room::HrtfSet(), kSampleRate,
                                                  SushiDSP::room::ListenerKind::Ears),
                 std::runtime_error);
    EXPECT_THROW(SushiDSP::room::build_render_set(garage, fallback_hrtf(), 44100.0,
                                                  SushiDSP::room::ListenerKind::Ears),
                 std::runtime_error);
}

TEST(Unit_Dsp, BuildRenderSetTakesTheDirectPairFromTheHrtfSet)
{
    // A source one metre to the listener's left, at ear height, sits at 90 degrees azimuth on
    // the spherical head: Woodworth's (a / c) (theta + sin theta) with a = 0.0875 m and theta =
    // pi / 2 is 0.0006558 s, 31.48 frames, and the right ear is the later one.
    SushiDSP::room::RoomScene scene = SushiDSP::test::garage_scene();
    scene.listener_position = { 4.0, 2.0, 1.2 };
    scene.listener_yaw_rad = 0.0;
    scene.source_position = { 4.0, 3.0, 1.2 };
    const std::unique_ptr<SushiDSP::room::RoomRenderSet> set = SushiDSP::room::build_render_set(
        scene, fallback_hrtf(), kSampleRate, SushiDSP::room::ListenerKind::Ears);

    const double woodworth_frames =
        SushiDSP::room::SphericalHeadHrtf::kHeadRadiusMetres / kSpeedOfSound
        * (0.5 * kPi + std::sin(0.5 * kPi)) * kSampleRate;
    EXPECT_NEAR(woodworth_frames, 31.479, 1e-3);
    EXPECT_NEAR(set->direct_itd_frames, woodworth_frames, 0.1);

    const SushiDSP::room::HrtfSet::Pair pair = fallback_hrtf().interpolate(0.5 * kPi, 0.0);
    EXPECT_EQ(set->direct_left, pair.left);
    EXPECT_EQ(set->direct_right, pair.right);
}

TEST(Unit_Dsp, BuildRenderSetCarriesTheDiffuseFieldPair)
{
    SushiDSP::room::HrtfSet hrtf = fallback_hrtf();
    std::array<float, SushiDSP::room::HrtfSet::kDiffuseTaps> left{};
    std::array<float, SushiDSP::room::HrtfSet::kDiffuseTaps> right{};
    for (std::uint32_t tap = 0; tap < SushiDSP::room::HrtfSet::kDiffuseTaps; ++tap)
    {
        left[tap] = static_cast<float>(tap);
        right[tap] = -static_cast<float>(tap);
    }
    hrtf.set_diffuse_field(left, right);

    const std::unique_ptr<SushiDSP::room::RoomRenderSet> set = SushiDSP::room::build_render_set(
        SushiDSP::test::garage_scene(), hrtf, kSampleRate, SushiDSP::room::ListenerKind::Ears);
    EXPECT_EQ(set->diffuse_left, left);
    EXPECT_EQ(set->diffuse_right, right);
}

TEST(Unit_Dsp, BuildRenderSetCostsUnderOnePublishBudget)
{
    const SushiDSP::room::RoomScene garage = SushiDSP::test::garage_scene();
    const SushiDSP::room::HrtfSet& hrtf = fallback_hrtf();

    const auto start = std::chrono::steady_clock::now();
    const std::unique_ptr<SushiDSP::room::RoomRenderSet> set = SushiDSP::room::build_render_set(
        garage, hrtf, kSampleRate, SushiDSP::room::ListenerKind::Ears);
    const auto stop = std::chrono::steady_clock::now();
    const double milliseconds = std::chrono::duration<double, std::milli>(stop - start).count();
    EXPECT_LT(milliseconds, kPublishBudgetMs)
        << "taps " << set->early.count << " modes " << set->modal.count;
}

TEST(Unit_Dsp, RoomRenderSetIsTriviallyCopyable)
{
    static_assert(std::is_trivially_copyable<SushiDSP::room::RoomRenderSet>::value,
                  "it crosses to the audio thread");
    SUCCEED();
}
