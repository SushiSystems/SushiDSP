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
 * @file test_room_render_set.cpp
 * @brief Tests for build_render_set, the join of the four solvers and the HRTF into one record.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

#include <SushiDSP/nodes/room_scenes/room_fixtures.hpp>
#include <SushiDSP/nodes/room/early_tap_set.hpp>
#include <SushiDSP/nodes/room/eyring_decay.hpp>
#include <SushiDSP/nodes/room/hrtf_set.hpp>
#include <SushiDSP/nodes/room/modal_design.hpp>
#include <SushiDSP/nodes/room/image_source_solver.hpp>
#include <SushiDSP/nodes/room/room_host.hpp>
#include <SushiDSP/nodes/room/modal_solver.hpp>
#include <SushiDSP/nodes/room/room_render_set.hpp>
#include <SushiDSP/nodes/room/room_scene.hpp>
#include <SushiDSP/nodes/room/spherical_head_hrtf.hpp>

#include "support/tolerance.hpp"

namespace
{
    using SushiDSP::room::ListenerKind;
    using SushiDSP::room::RoomRenderSet;
    using SushiDSP::room::RoomScene;

    /// @brief Holds pi.
    constexpr double K_PI = 3.14159265358979323846;

    /// @brief Holds the sample rate every set in this file is built at.
    constexpr double K_SAMPLE_RATE = 48000.0;

    /// @brief Holds the speed of sound the delays are derived with, in metres per second.
    constexpr double K_SPEED_OF_SOUND = 343.0;

    /// @brief Holds the ratio of error to bound above which a derived check fails.
    constexpr double K_LIMIT = TestSupport::K_DEFAULT_THRESHOLD;

    /// @brief Holds the frames the direct path's interaural delay may sit from the Woodworth
    /// figure. Characterised, not derived: the fallback set measures the delay from its own
    /// responses.
    constexpr double K_ITD_TOLERANCE_FRAMES = 0.1;

    /// @brief Holds the largest interaural delay of the fallback head in frames, the Woodworth
    /// figure at 90 degrees, which bounds the rounding of a delay of any direction.
    constexpr double K_MAX_ITD_FRAMES = 31.479;

    /// @brief Returns half a unit of the last digit of a figure typed with the given number of
    /// decimals.
    double half_unit_of_last_digit(int decimals)
    {
        return 0.5 * std::pow(10.0, -decimals);
    }

    /// @brief Returns how many times its bound a float differs from a reference that took the
    /// given number of rounded operations.
    double ratio_float(double got, double reference, std::size_t operations)
    {
        return TestSupport::measure_relative_error<float>(got, reference, operations).ratio;
    }

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

    /// @brief Builds the render set of a scene at K_SAMPLE_RATE from the fallback head.
    std::unique_ptr<RoomRenderSet> build(const RoomScene& scene, ListenerKind listener)
    {
        return SushiDSP::room::build_render_set(scene, fallback_hrtf(), K_SAMPLE_RATE, listener);
    }

    /// @brief Returns the message a refused scene is refused with, or a note that it was not.
    std::string refusal_message(const RoomScene& scene)
    {
        try
        {
            build(scene, ListenerKind::Ears);
        }
        catch (const std::runtime_error& error)
        {
            return error.what();
        }
        return "the scene was accepted";
    }

    /// @brief Returns a ramp of the diffuse-field length that counts up from 0, negated for a
    /// negative sign.
    std::array<float, SushiDSP::room::HrtfSet::K_DIFFUSE_TAPS> ramp(float sign)
    {
        std::array<float, SushiDSP::room::HrtfSet::K_DIFFUSE_TAPS> taps{};
        std::iota(taps.begin(), taps.end(), 0.0f);
        for (float& tap : taps)
        {
            tap *= sign;
        }
        return taps;
    }

    /// @brief Returns the garage with the source and listener in the corners the standing wave
    /// tests read, and the microphone at (1.0, 0.5, 0.5).
    RoomScene standing_wave_scene()
    {
        RoomScene scene = SushiDSP::test::garage_scene();
        scene.source_position = {0.4, 0.5, 0.5};
        scene.listener_position = {5.4, 3.4, 2.0};
        scene.microphone_position = {1.0, 0.5, 0.5};
        return scene;
    }

    /// @brief Holds the first three modes of the garage worked out by hand: where each stands in
    /// the ordered list and the pressure shape at the microphone and at the two ears. The
    /// garage is 6 x 4 x 2.5 m and the shape is the product of cos(pi n x / L) over the axes. The
    /// ears sit 0.085 m either side of the listener at (5.4, 3.4, 2.0) along y.
    struct StandingWave
    {
            const char* name;
            std::size_t index;
            double microphone;
            double left_ear;
            double right_ear;
    };

    /// @brief Holds the modes (1, 0, 0), (0, 1, 0) and (0, 0, 1): cos(pi / 6) = 0.8660254 and
    /// cos(0.9 pi) = -0.9510565 for x; cos(pi / 8) = 0.9238795 and cos(pi 3.485 / 4) = -0.9193071
    /// and cos(pi 3.315 / 4) = -0.8587364 for y; cos(pi / 5) = 0.8090170 and cos(0.8 pi) =
    /// -0.8090170 for z.
    const std::array<StandingWave, 3> kStandingWaves = {
        {{"AlongX", 0, 0.8660254037844387, -0.9510565162951535, -0.9510565162951535},
         {"AlongY", 1, 0.9238795325112867, -0.9193071413391818, -0.8587363943232506},
         {"AlongZ", 4, 0.8090169943749475, -0.8090169943749473, -0.8090169943749473}}};

    /// @brief Names a standing wave case after its axis.
    std::string wave_name(const ::testing::TestParamInfo<StandingWave>& info)
    {
        return info.param.name;
    }
} // namespace

class Small_RoomRenderSetGarage : public ::testing::Test
{
    protected:
        /// @brief Builds the garage's render set for the ears and for the microphone once.
        static void SetUpTestSuite()
        {
            garage = SushiDSP::test::garage_scene();
            ears = build(garage, ListenerKind::Ears);
            microphone = build(garage, ListenerKind::Microphone);
        }

        /// @brief Frees the sets.
        static void TearDownTestSuite()
        {
            ears.reset();
            microphone.reset();
        }

        /// @brief Stores the garage.
        inline static RoomScene garage;

        /// @brief Stores the set built for the ears.
        inline static std::unique_ptr<RoomRenderSet> ears;

        /// @brief Stores the set built for the microphone.
        inline static std::unique_ptr<RoomRenderSet> microphone;
};

TEST_F(Small_RoomRenderSetGarage, EarlyLayerIsTheSolversOrderFourSetForTheEars)
{
    // The garage keeps 96 of the 129 order-4 images. See tests/nodes/room/README.md.
    SushiDSP::room::ImageSourceSolver::Options options;
    options.max_order = 4;
    options.listener = ListenerKind::Ears;
    options.sample_rate = K_SAMPLE_RATE;
    const SushiDSP::room::EarlyTapSet solved =
        SushiDSP::room::ImageSourceSolver::solve(garage, options);
    ASSERT_NE(ears, nullptr);
    EXPECT_EQ(ears->early.count, solved.count);
    EXPECT_EQ(ears->early.crossover_time_s, solved.crossover_time_s);
}

TEST_F(Small_RoomRenderSetGarage, EarlyLayerHoldsTapsInsideItsCapacity)
{
    EXPECT_GT(ears->early.count, 0u);
    EXPECT_LE(ears->early.count, SushiDSP::room::EarlyTapSet::K_MAX_EARLY_TAPS);
}

TEST_F(Small_RoomRenderSetGarage, LateLayerIsFilled)
{
    EXPECT_GT(ears->late.line_frames[0], 0u);
    EXPECT_GT(ears->late.input_gain, 0.0f);
}

TEST_F(Small_RoomRenderSetGarage, ModalLayerHoldsOneHundredAndEightySevenModes)
{
    // 187 modes lie under the garage's Schroeder frequency, 2000 sqrt(1.1608 / 60) = 278.18 Hz.
    EXPECT_EQ(ears->modal.count, 187u);
}

TEST_F(Small_RoomRenderSetGarage, CrossoverIsTheWorkedOutSchroederFrequency)
{
    EXPECT_NEAR(ears->crossover_hz, 278.18, half_unit_of_last_digit(2));
}

TEST_F(Small_RoomRenderSetGarage, CrossoverIsTheSchroederFrequencyOfTheRoom)
{
    EXPECT_LE(ratio_float(ears->crossover_hz,
                          SushiDSP::room::EyringDecay::schroeder_frequency_hz(garage), 2),
              K_LIMIT);
}

TEST_F(Small_RoomRenderSetGarage, CrossoverIsTheModalLayersCrossover)
{
    EXPECT_EQ(ears->crossover_hz, ears->modal.crossover_hz);
}

TEST_F(Small_RoomRenderSetGarage, SetRecordsItsListenerKind)
{
    EXPECT_EQ(ears->listener, ListenerKind::Ears);
    EXPECT_EQ(microphone->listener, ListenerKind::Microphone);
}

TEST_F(Small_RoomRenderSetGarage, DirectPathIsDelayedByItsPropagationTime)
{
    // 2.596151 m is 363 frames and a gain of 0.385186. See tests/nodes/room/README.md.
    EXPECT_EQ(ears->direct_delay_frames, 363.0f);
}

TEST_F(Small_RoomRenderSetGarage, SourceIsTheWorkedOutDistanceFromTheListener)
{
    EXPECT_NEAR(distance_between(garage.source_position, garage.listener_position), 2.596151,
                half_unit_of_last_digit(6));
}

TEST_F(Small_RoomRenderSetGarage, DirectGainIsTheWorkedOutFigure)
{
    EXPECT_NEAR(ears->direct_gain, 0.385186, half_unit_of_last_digit(6) + 4 * TestSupport::unit_roundoff<float>());
}

TEST_F(Small_RoomRenderSetGarage, DirectGainIsOneOverTheDistance)
{
    const double distance = distance_between(garage.source_position, garage.listener_position);
    EXPECT_LE(ratio_float(ears->direct_gain, 1.0 / distance, 3), K_LIMIT);
}

TEST_F(Small_RoomRenderSetGarage, SourceStraightAheadIsHeardByBothEarsAtTheSameTime)
{
    // Straight ahead in the median plane the two ears hear the source at the same time.
    const double bound = 4.0 * TestSupport::unit_roundoff<float>() * K_MAX_ITD_FRAMES;
    EXPECT_LE(std::fabs(static_cast<double>(ears->direct_itd_frames)), bound);
}

TEST_F(Small_RoomRenderSetGarage, MicrophoneDirectPathIsDelayedByItsPropagationTime)
{
    // 0.1 m is 14 frames and a gain of 10. See tests/nodes/room/README.md.
    EXPECT_EQ(microphone->direct_delay_frames, 14.0f);
}

TEST_F(Small_RoomRenderSetGarage, MicrophoneDirectGainIsTenAtAHundredMillimetres)
{
    EXPECT_LE(ratio_float(microphone->direct_gain, 10.0, 3), K_LIMIT);
}

TEST_F(Small_RoomRenderSetGarage, MicrophoneFirstTapEncodesTheOmnidirectionalChannelAtUnity)
{
    EXPECT_LE(ratio_float(microphone->early.taps[0].encode[0], 1.0, 2), K_LIMIT);
}

TEST(Medium_RoomRenderSet, BuildRenderSetRefusesAnInvalidSceneAndNamesTheDimension)
{
    RoomScene scene = SushiDSP::test::garage_scene();
    scene.dimensions.x = 15.0;
    EXPECT_NE(refusal_message(scene).find("dimension x"), std::string::npos)
        << refusal_message(scene);
}

TEST(Medium_RoomRenderSet, BuildRenderSetRefusesAnInvalidSceneAndNamesTheLimit)
{
    RoomScene scene = SushiDSP::test::garage_scene();
    scene.dimensions.x = 15.0;
    EXPECT_NE(refusal_message(scene).find("12.0"), std::string::npos) << refusal_message(scene);
}

TEST(Medium_RoomRenderSet, BuildRenderSetRefusesAnHrtfSetThatDoesNotFit)
{
    const RoomScene garage = SushiDSP::test::garage_scene();
    EXPECT_THROW(SushiDSP::room::build_render_set(garage, SushiDSP::room::HrtfSet(), K_SAMPLE_RATE,
                                                  ListenerKind::Ears),
                 std::runtime_error);
    EXPECT_THROW(SushiDSP::room::build_render_set(garage, fallback_hrtf(), 44100.0,
                                                  ListenerKind::Ears),
                 std::runtime_error);
}

class Medium_RoomRenderSetDirectPair : public ::testing::Test
{
    protected:
        /// @brief Builds the set for a source one metre to the left of a listener facing +x. The
        /// set carries the spherical head the claims read, 2.4 s on the Debug build, so the suite is
        /// `Medium_`.
        void SetUp() override
        {
            RoomScene scene = SushiDSP::test::garage_scene();
            scene.listener_position = {4.0, 2.0, 1.2};
            scene.listener_yaw_rad = 0.0;
            scene.source_position = {4.0, 3.0, 1.2};
            set = build(scene, ListenerKind::Ears);
        }

        /// @brief Stores the set.
        std::unique_ptr<RoomRenderSet> set;
};

TEST(Small_RoomRenderSet, WoodworthDelayOfTheFallbackHeadAtNinetyDegreesIsTheWorkedOutFigure)
{
    // The interaural delay is 0.0006558 s, 31.48 frames. See tests/nodes/room/README.md.
    const double woodworth_frames = SushiDSP::room::SphericalHeadHrtf::K_HEAD_RADIUS_METRES /
                                    K_SPEED_OF_SOUND * (0.5 * K_PI + std::sin(0.5 * K_PI)) *
                                    K_SAMPLE_RATE;
    EXPECT_NEAR(woodworth_frames, K_MAX_ITD_FRAMES, half_unit_of_last_digit(3));
}

TEST_F(Medium_RoomRenderSetDirectPair, DirectDelayIsTheWoodworthFigureForASourceToTheSide)
{
    const double woodworth_frames = SushiDSP::room::SphericalHeadHrtf::K_HEAD_RADIUS_METRES /
                                    K_SPEED_OF_SOUND * (0.5 * K_PI + std::sin(0.5 * K_PI)) *
                                    K_SAMPLE_RATE;
    EXPECT_NEAR(set->direct_itd_frames, woodworth_frames, K_ITD_TOLERANCE_FRAMES);
}

TEST_F(Medium_RoomRenderSetDirectPair, LeftResponseIsTheHrtfSetsAtTheSourceDirection)
{
    EXPECT_EQ(set->direct_left, fallback_hrtf().interpolate(0.5 * K_PI, 0.0).left);
}

TEST_F(Medium_RoomRenderSetDirectPair, RightResponseIsTheHrtfSetsAtTheSourceDirection)
{
    EXPECT_EQ(set->direct_right, fallback_hrtf().interpolate(0.5 * K_PI, 0.0).right);
}

TEST(Medium_RoomRenderSet, BuildRenderSetCarriesTheDiffuseFieldPair)
{
    SushiDSP::room::HrtfSet hrtf = fallback_hrtf();
    hrtf.set_diffuse_field(ramp(1.0f), ramp(-1.0f));

    const std::unique_ptr<RoomRenderSet> set = SushiDSP::room::build_render_set(
        SushiDSP::test::garage_scene(), hrtf, K_SAMPLE_RATE, ListenerKind::Ears);
    EXPECT_EQ(set->diffuse_left, ramp(1.0f));
    EXPECT_EQ(set->diffuse_right, ramp(-1.0f));
}

TEST_F(Small_RoomRenderSetGarage, EarlyAndModalCountsStayInsideTheirCapacities)
{
    EXPECT_LE(ears->early.count, SushiDSP::room::EarlyTapSet::K_MAX_EARLY_TAPS);
    EXPECT_LE(ears->modal.count, SushiDSP::room::ModalDesign::K_MAX_MODES);
}

TEST_F(Small_RoomRenderSetGarage, TwoBuildsHoldTheSameTapAndModeCounts)
{
    const std::unique_ptr<RoomRenderSet> second = build(garage, ListenerKind::Ears);
    EXPECT_EQ(second->early.count, ears->early.count);
    EXPECT_EQ(second->modal.count, ears->modal.count);
}

class Small_RoomRenderSetStandingWaves : public ::testing::TestWithParam<StandingWave>
{
    protected:
        /// @brief Builds the standing wave scene once for the microphone and for the ears.
        static void SetUpTestSuite()
        {
            const RoomScene scene = standing_wave_scene();
            microphone = build(scene, ListenerKind::Microphone);
            ears = build(scene, ListenerKind::Ears);
        }

        /// @brief Frees the sets.
        static void TearDownTestSuite()
        {
            microphone.reset();
            ears.reset();
        }

        /// @brief Stores the set built for the microphone.
        inline static std::unique_ptr<RoomRenderSet> microphone;

        /// @brief Stores the set built for the ears.
        inline static std::unique_ptr<RoomRenderSet> ears;
};

TEST_P(Small_RoomRenderSetStandingWaves, MicrophoneSetReadsTheModeAtTheMicrophoneForBothEars)
{
    // The modal field is read at the set's receiver. See tests/nodes/room/README.md.
    ASSERT_GT(microphone->modal.count, GetParam().index);
    const auto& gain = microphone->modal.modes[GetParam().index].ear_gain;
    EXPECT_LE(ratio_float(gain[0], GetParam().microphone, 4), K_LIMIT);
    EXPECT_LE(ratio_float(gain[1], GetParam().microphone, 4), K_LIMIT);
}

TEST_P(Small_RoomRenderSetStandingWaves, EarsSetReadsTheModeAtTheLeftEar)
{
    ASSERT_GT(ears->modal.count, GetParam().index);
    EXPECT_LE(ratio_float(ears->modal.modes[GetParam().index].ear_gain[0], GetParam().left_ear, 4),
              K_LIMIT);
}

TEST_P(Small_RoomRenderSetStandingWaves, EarsSetReadsTheModeAtTheRightEar)
{
    ASSERT_GT(ears->modal.count, GetParam().index);
    EXPECT_LE(ratio_float(ears->modal.modes[GetParam().index].ear_gain[1], GetParam().right_ear, 4),
              K_LIMIT);
}

TEST_P(Small_RoomRenderSetStandingWaves, EarsSetAndMicrophoneSetShareTheResonators)
{
    ASSERT_GT(ears->modal.count, GetParam().index);
    ASSERT_GT(microphone->modal.count, GetParam().index);
    EXPECT_EQ(ears->modal.modes[GetParam().index].section,
              microphone->modal.modes[GetParam().index].section);
}

INSTANTIATE_TEST_SUITE_P(Modes, Small_RoomRenderSetStandingWaves, ::testing::ValuesIn(kStandingWaves),
                         wave_name);

TEST(Medium_RoomRenderSet, EarsAndMicrophoneSetsHoldTheSameNumberOfModes)
{
    const RoomScene scene = standing_wave_scene();
    EXPECT_EQ(build(scene, ListenerKind::Ears)->modal.count,
              build(scene, ListenerKind::Microphone)->modal.count);
}
