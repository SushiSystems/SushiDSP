/**************************************************************************/
/* test_microphone.cpp                                                    */
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
 * @file test_microphone.cpp
 * @brief Tests for RoomNode's microphone port: that it ignores the listener, the bass a close
 * cardioid lifts, the room it hears against the ears, and the 1 / r level on the cabinet's axis.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include <SushiDSP/nodes/room/air_absorption.hpp>
#include <SushiDSP/nodes/room/microphone_model.hpp>
#include <SushiDSP/nodes/room/room_node.hpp>

#include "nodes/room/test_room_node/room_node_rig.hpp"

namespace
{
    using namespace SushiDSP::test::room_node_rig;
    using SushiDSP::room::RoomScene;

    /// @brief Holds the blocks the listener-move render runs and the block the listener moves
    /// before: the move is adopted at the next block, which leaves four rendered after it.
    constexpr std::uint32_t K_LISTENER_MOVE_BLOCKS = 6;

    /// @brief Holds the block the listener moves before.
    constexpr std::uint32_t K_LISTENER_MOVE_BLOCK = 2;

    /// @brief Holds the capsule distance of the close microphone, in metres.
    constexpr double K_CLOSE_M = 0.05;

    /// @brief Holds the capsule distance of the distant microphone, in metres.
    constexpr double K_DISTANT_M = 3.0;

    /// @brief Holds the band the bottom end is read in, in hertz.
    constexpr double K_BASS_BAND_HZ = 125.0;

    /// @brief Holds the band under it that the model lifts more, in hertz.
    constexpr double K_LOWER_BASS_BAND_HZ = 100.0;

    /// @brief Holds the band the tilt is read against, in hertz.
    constexpr double K_REFERENCE_BAND_HZ = 4000.0;

    /// @brief Holds the tilt the model gives at K_BASS_BAND_HZ, to the two decimals it is quoted
    /// at, in dB.
    constexpr double K_MODEL_TILT_125_DB = 13.68;

    /// @brief Holds the tilt the model gives at K_LOWER_BASS_BAND_HZ, to the two decimals it is
    /// quoted at, in dB.
    constexpr double K_MODEL_TILT_100_DB = 15.39;

    /// @brief Holds half a unit of the last quoted decimal of the model's tilt, in dB.
    constexpr double K_MODEL_QUOTE_DB = 0.005;

    /// @brief Bounds how far the rendered tilt is from the model's, in dB.
    constexpr double K_TILT_BAND_DB = 1.0;

    /// @brief Holds the least the microphone's direct-to-reverberant ratio exceeds the ears', in dB.
    constexpr double K_DIRECT_TO_REVERBERANT_LIFT_DB = 11.5;

    /// @brief Holds the blocks of the direct-to-reverberant render: 128 blocks, 0.68 s. The
    /// garage's slowest decay is 1.9 s, so a render this long leaves 10^(-6 * 0.68 / 1.9) = 0.7 %
    /// of the reverberant energy past it, 0.03 dB on each ratio.
    constexpr std::uint32_t K_RATIO_BLOCKS = 128;

    /// @brief Holds the frames of the direct window of the ratio, from two frames before the
    /// onset: 62 frames.
    constexpr std::size_t K_RATIO_DIRECT_FRAMES = 62;

    /// @brief Holds the frames before the onset the ratio's direct window opens.
    constexpr std::size_t K_RATIO_LEAD_FRAMES = 2;

    /// @brief Bounds how far an omni capsule's direct level is from 1 / r, in dB. The air takes up
    /// to 1.3 dB over 4 m at 4 kHz.
    constexpr double K_LEVEL_BAND_DB = 1.5;

    /// @brief Bounds how far a doubling of the distance is from 6.02 dB, in dB.
    constexpr double K_DOUBLING_BAND_DB = 1.0;

    /// @brief Returns the cardioid's gain in dB at one frequency and distance.
    double cardioid_db(double frequency_hz, double distance_m)
    {
        return 20.0 * std::log10(0.5 + 0.5 * SushiDSP::room::MicrophoneModel::proximity_boost(
                                                 frequency_hz, distance_m));
    }

    /// @brief Returns the air's loss between the first and the last band over one path, in dB.
    double air_db(double distance_m)
    {
        const SushiDSP::room::BandResponse air = SushiDSP::room::AirAbsorption::over_path(distance_m);
        return 20.0 * std::log10(static_cast<double>(air.gain[0]) / air.gain[5]);
    }

    /// @brief Returns the model's tilt of a band against K_REFERENCE_BAND_HZ between the close
    /// and the distant capsule, in dB.
    double model_tilt_db(double band_hz)
    {
        return cardioid_db(band_hz, K_CLOSE_M) - cardioid_db(K_REFERENCE_BAND_HZ, K_CLOSE_M) -
               (cardioid_db(band_hz, K_DISTANT_M) - cardioid_db(K_REFERENCE_BAND_HZ, K_DISTANT_M)) +
               air_db(K_DISTANT_M) - air_db(K_CLOSE_M);
    }

    /// @brief Returns the dead room with a point cabinet and the cardioid at one distance, whose
    /// window holds the direct path alone.
    RoomScene point_dead_room(double microphone_x)
    {
        RoomScene scene = centred_dead_room();
        scene.cabinet = point_cabinet();
        scene.microphone_position = {microphone_x, 6.0, 6.0};
        return scene;
    }

    /// @brief Returns the close cardioid's direct spectrum, made once.
    const std::vector<double>& close_spectrum()
    {
        static const std::vector<double> spectrum =
            direct_power_db(point_dead_room(6.0 + K_CLOSE_M), K_LISTENER_MICROPHONE);
        return spectrum;
    }

    /// @brief Returns the distant cardioid's direct spectrum, made once.
    const std::vector<double>& distant_spectrum()
    {
        static const std::vector<double> spectrum =
            direct_power_db(point_dead_room(6.0 + K_DISTANT_M), K_LISTENER_MICROPHONE);
        return spectrum;
    }

    /// @brief Returns the rendered tilt of a band against K_REFERENCE_BAND_HZ between the close
    /// and the distant capsule, in dB.
    double rendered_tilt_db(double band_hz)
    {
        const auto tilt = [band_hz](const std::vector<double>& power)
        {
            return third_octave_db(power, band_hz) - third_octave_db(power, K_REFERENCE_BAND_HZ);
        };
        return tilt(close_spectrum()) - tilt(distant_spectrum());
    }

    /// @brief Returns the direct-to-reverberant ratio of an impulse response in decibels: the
    /// energy from two frames before the direct onset to 60 frames after it, against all the
    /// rest, which includes the standing waves that ring from frame 0.
    double direct_to_reverberant_db(const std::vector<float>& response, double onset_frames)
    {
        const std::size_t first =
            static_cast<std::size_t>(std::floor(onset_frames)) - K_RATIO_LEAD_FRAMES;
        const double direct = energy(response, first, first + K_RATIO_DIRECT_FRAMES);
        const double total = energy(response, 0, response.size());
        return 10.0 * std::log10(direct / (total - direct));
    }

    /// @brief Returns the dB the omni capsule's direct window holds at one distance on the
    /// cabinet's axis in the dead room.
    double direct_level_db(double distance_m)
    {
        RoomScene scene = point_dead_room(6.0 + distance_m);
        scene.microphone_pattern = SushiDSP::room::PolarPattern::Omni;
        const std::vector<float> out = render(*prepared_node(scene, K_LISTENER_MICROPHONE),
                                              impulse(K_DIRECT_WINDOW_BLOCKS * K_BLOCK));
        return 10.0 * std::log10(std::max(energy(out, 0, K_DIRECT_WINDOW), 1e-30));
    }
} // namespace

TEST(Medium_RoomNodeMicrophone, RoomNodeMicrophoneIgnoresAListenerMovedMidRender)
{
    // A moved listener changes nothing the microphone hears, so the two ports agree bit for bit.
    RoomScene scene = wave_scene();
    scene.listener_position = {2.0, 1.0, 1.0};
    scene.microphone_pattern = SushiDSP::room::PolarPattern::Cardioid;
    RoomScene listener_moved = scene;
    listener_moved.listener_position = {4.0, 3.0, 1.5};
    std::vector<float> input = tone(K_LISTENER_MOVE_BLOCKS * K_BLOCK);
    input[0] = 1.0f;

    std::unique_ptr<SushiDSP::room::RoomNode> still = prepared_node(scene, K_LISTENER_MICROPHONE);
    std::unique_ptr<SushiDSP::room::RoomNode> moved = prepared_node(scene, K_LISTENER_MICROPHONE);
    const std::vector<float> heard_still = render(*still, input);
    const std::vector<float> heard_moved =
        render(*moved, input, {K_LISTENER_MOVE_BLOCK}, scene_edit(listener_moved));

    EXPECT_EQ(heard_moved, heard_still);
}

TEST(Small_RoomNodeMicrophoneModel, MicrophoneModelTiltsTheBottomEndByTheQuotedFigureAt125Hz)
{
    EXPECT_NEAR(model_tilt_db(K_BASS_BAND_HZ), K_MODEL_TILT_125_DB, K_MODEL_QUOTE_DB);
}

TEST(Small_RoomNodeMicrophoneModel, MicrophoneModelTiltsTheBottomEndByTheQuotedFigureAt100Hz)
{
    EXPECT_NEAR(model_tilt_db(K_LOWER_BASS_BAND_HZ), K_MODEL_TILT_100_DB, K_MODEL_QUOTE_DB);
}

TEST(Medium_RoomNodeMicrophoneProximity, RoomNodeMicrophoneAtFiveCentimetresLiftsTheBottomEnd)
{
    // The point cabinet is one 1 cm cone, so the capsule hears a point.
    const double measured = rendered_tilt_db(K_BASS_BAND_HZ);
    RecordProperty("measured_db", measured);
    EXPECT_NEAR(measured, model_tilt_db(K_BASS_BAND_HZ), K_TILT_BAND_DB);
}

TEST(Medium_RoomNodeMicrophoneProximity, RoomNodeMicrophoneLiftsTheBottomEndMoreAt100Hz)
{
    // The fit is anchored at 125 Hz and its low shelf holds level; the render read 13.79 dB there.
    EXPECT_GT(rendered_tilt_db(K_LOWER_BASS_BAND_HZ), rendered_tilt_db(K_BASS_BAND_HZ));
}

TEST(Medium_RoomNodeMicrophoneRoom, RoomNodeMicrophoneHearsLessRoomThanTheEars)
{
    // Between the four cones the difference measures 12.20 dB.
    RoomScene scene = SushiDSP::test::garage_scene();
    scene.source_position = {1.5, 2.0, 0.5};
    scene.microphone_position = {1.55, 2.0, 0.5};
    scene.microphone_yaw_rad = K_PI;
    scene.microphone_pattern = SushiDSP::room::PolarPattern::Cardioid;
    scene.listener_position = {3.5, 2.0, 1.2};

    const std::vector<float> input = impulse(K_RATIO_BLOCKS * K_BLOCK);
    const std::vector<float> microphone = render(*prepared_node(scene, K_LISTENER_MICROPHONE), input);
    const std::vector<float> ears = render(*prepared_node(scene, K_LISTENER_EARS), input);

    const double microphone_db = direct_to_reverberant_db(microphone, 0.05 * K_FRAMES_PER_METRE);
    const double ears_db =
        direct_to_reverberant_db(ears, compensated_direct_frames(std::sqrt(4.0 + 0.49)));
    RecordProperty("microphone_db", microphone_db);
    RecordProperty("ears_db", ears_db);
    EXPECT_GE(microphone_db - ears_db, K_DIRECT_TO_REVERBERANT_LIFT_DB);
}

class Medium_RoomNodeOneOverR : public ::testing::TestWithParam<double>
{
};

TEST_P(Medium_RoomNodeOneOverR, RoomNodeCarriesOneOverROnTheCabinetsAxis)
{
    // The one level in the module that comes from geometry rather than from DiffuseField.
    EXPECT_NEAR(direct_level_db(GetParam()), 20.0 * std::log10(1.0 / GetParam()), K_LEVEL_BAND_DB);
}

INSTANTIATE_TEST_SUITE_P(Metres, Medium_RoomNodeOneOverR, ::testing::Values(1.0, 2.0, 4.0));

class Medium_RoomNodeDoubling : public ::testing::TestWithParam<double>
{
};

TEST_P(Medium_RoomNodeDoubling, RoomNodeLosesSixDecibelsPerDoublingOfTheDistance)
{
    EXPECT_NEAR(direct_level_db(GetParam()) - direct_level_db(2.0 * GetParam()),
                20.0 * std::log10(2.0), K_DOUBLING_BAND_DB);
}

INSTANTIATE_TEST_SUITE_P(Metres, Medium_RoomNodeDoubling, ::testing::Values(1.0, 2.0));
