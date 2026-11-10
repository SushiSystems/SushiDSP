/**************************************************************************/
/* test_seam.cpp                                                          */
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
 * @file test_seam.cpp
 * @brief Tests for RoomNode: the seam between the wave layer and the geometric one, the
 * crossover the node reports, and the dead room that switches the wave layer off.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <SushiDSP/nodes/room/diffuse_field.hpp>
#include <SushiDSP/nodes/room/early_reflection_renderer.hpp>
#include <SushiDSP/nodes/room/eyring_decay.hpp>
#include <SushiDSP/nodes/room/late_reverb.hpp>
#include <SushiDSP/nodes/room/modal_solver.hpp>
#include <SushiDSP/nodes/room/room_node.hpp>
#include <SushiDSP/nodes/room/room_render_set.hpp>
#include <SushiDSP/nodes/room/spherical_head_hrtf.hpp>
#include <SushiDSP/nodes/room/stage_limits.hpp>

#include "nodes/room/test_room_node/room_node_rig.hpp"
#include "support/tolerance.hpp"

namespace
{
    using namespace SushiDSP::test::room_node_rig;
    using SushiDSP::room::RoomScene;

    /// @brief Holds the frames a medium seam render runs for: 0.34 s. The slowest decay of the
    /// garage is 1.9 s, which leaves 10^(-6 * 0.34 / 1.9) = 8 % of the energy past the render,
    /// 0.36 dB under the figure a longer render reads. A band's mean power is the band's energy by
    /// Parseval, so the coarser bins of 2.9 Hz do not move it. The cross terms of the octave's
    /// overlapping modes scatter per pose; see the README.
    constexpr std::uint32_t K_MEDIUM_FRAMES = 16384;

    /// @brief Holds the frames a large seam render runs for: 0.68 s, 10^(-6 * 0.68 / 1.9) = 0.7 %
    /// of the energy past it, 0.03 dB.
    constexpr std::uint32_t K_LARGE_FRAMES = 32768;

    /// @brief Bounds how far a layer's rendered level is from the level it is designed to carry,
    /// in dB.
    constexpr double K_SEAM_BAND_DB = 2.0;

    /// @brief Holds the poses one room's seam is read at, as fractions of its three lengths:
    /// the cabinet's then the listener's, none on a symmetry plane, cabinets near the floor.
    constexpr std::array<std::array<double, 6>, 6> K_SEAM_POSES = {{
        {0.07, 0.12, 0.20, 0.90, 0.85, 0.80},
        {0.30, 0.20, 0.25, 0.65, 0.70, 0.55},
        {0.15, 0.80, 0.30, 0.55, 0.30, 0.65},
        {0.45, 0.35, 0.18, 0.80, 0.60, 0.45},
        {0.85, 0.15, 0.22, 0.25, 0.75, 0.60},
        {0.60, 0.88, 0.28, 0.20, 0.35, 0.70},
    }};

    /// @brief Holds the poses the medium seam test averages: every pose of the table, as the large
    /// cases do.
    constexpr std::size_t K_MEDIUM_POSES = K_SEAM_POSES.size();

    /// @brief Holds one octave's rendered power and the power the design says it holds.
    struct OctavePower
    {
            /// @brief Stores the rendered mean power per hertz.
            double rendered = 0.0;

            /// @brief Stores the mean power per hertz the design predicts.
            double expected = 0.0;
    };

    /// @brief Returns the fourth-order Butterworth low half's power at f for a crossover at
    /// fc, 1 / (1 + (f / fc)^8); the high half's is (f / fc)^8 times it, so the two sum to one.
    /// The pair splits a modal field from a geometric one, which are uncorrelated, so it keeps
    /// the power identity rather than the amplitude one a Linkwitz-Riley pair keeps.
    double butterworth_low_power(double f, double fc)
    {
        return 1.0 / (1.0 + std::pow(f / fc, 8.0));
    }

    /// @brief Returns the mean of a power spectrum of the given size over [lower, upper) hertz.
    double seam_band_power(const std::vector<double>& power_db, std::uint32_t size, double lower,
                           double upper)
    {
        const double bin = K_SAMPLE_RATE / size;
        const auto first = static_cast<std::size_t>(std::ceil(lower / bin));
        const auto last = static_cast<std::size_t>(std::ceil(upper / bin));
        double sum = 0.0;
        for (std::size_t index = first; index < last; ++index)
        {
            sum += std::pow(10.0, power_db[index] / 10.0);
        }
        return sum / static_cast<double>(last - first);
    }

    /// @brief Returns the modal octave's power per hertz the design holds at its pose: each
    /// mode under f_s / 2 left out, the rest summed as the section's white-noise gain
    /// 2 / (1 - r^2) times b0^2 and psi^2 at the cabinet and the receiver, through the low
    /// half at the mode's frequency, times fs / 2 over the octave's width.
    double modal_pose_power(const SushiDSP::room::RoomRenderSet& set, const RoomScene& scene)
    {
        const double crossover = set.crossover_hz;
        const std::vector<SushiDSP::room::ModalSolver::Mode> modes =
            SushiDSP::room::ModalSolver::enumerate(scene, crossover);
        double sum = 0.0;
        for (std::size_t index = 0; index < modes.size() && index < set.modal.count; ++index)
        {
            if (modes[index].frequency_hz < 0.5 * crossover)
            {
                continue;
            }
            const SushiDSP::room::ModalDesign::Mode& mode = set.modal.modes[index];
            const double psi = static_cast<double>(mode.source_gain) * mode.source_gain *
                               static_cast<double>(mode.ear_gain[0]) * mode.ear_gain[0];
            sum += mode.section[0] * mode.section[0] * 2.0 / (1.0 - mode.section[4]) * psi *
                   butterworth_low_power(modes[index].frequency_hz, crossover);
        }
        return sum * (0.5 * K_SAMPLE_RATE) / (0.5 * crossover);
    }

    /// @brief Returns the geometric octave's power per hertz the reference holds: the diffuse
    /// field through the high half, averaged over the octave above f_s on the render's bins.
    double geometric_reference_power(const SushiDSP::room::DiffuseField& field, double crossover,
                                     std::uint32_t size)
    {
        const double bin = K_SAMPLE_RATE / size;
        const auto first = static_cast<std::size_t>(std::ceil(crossover / bin));
        const auto last = static_cast<std::size_t>(std::ceil(2.0 * crossover / bin));
        double sum = 0.0;
        for (std::size_t index = first; index < last; ++index)
        {
            const double f = static_cast<double>(index) * bin;
            sum += field.power_at(f) * std::pow(f / crossover, 8.0) *
                   butterworth_low_power(f, crossover);
        }
        return sum / static_cast<double>(last - first);
    }

    /// @brief Renders one room's microphone response at the first poses poses and returns the two
    /// octaves' rendered and expected powers, each averaged over them as energy.
    std::array<OctavePower, 2> seam_octaves(const RoomScene& room, std::size_t poses,
                                            std::uint32_t frames)
    {
        const SushiDSP::room::HrtfSet hrtf = SushiDSP::room::SphericalHeadHrtf::build(K_SAMPLE_RATE);
        const SushiDSP::room::DiffuseField field(room);
        const SushiDSP::room::Vec3 length = room.dimensions;
        std::array<OctavePower, 2> octaves{};
        for (std::size_t index = 0; index < poses; ++index)
        {
            const std::array<double, 6>& pose = K_SEAM_POSES[index];
            RoomScene scene = room;
            scene.source_position = {pose[0] * length.x, pose[1] * length.y, pose[2] * length.z};
            scene.listener_position = {pose[3] * length.x, pose[4] * length.y, pose[5] * length.z};
            scene.microphone_position = scene.listener_position;
            scene.microphone_pattern = SushiDSP::room::PolarPattern::Omni;
            const std::unique_ptr<SushiDSP::room::RoomRenderSet> set =
                SushiDSP::room::build_render_set(scene, hrtf, K_SAMPLE_RATE,
                                                 SushiDSP::room::ListenerKind::Microphone);
            const double crossover = set->crossover_hz;
            std::unique_ptr<SushiDSP::room::RoomNode> node =
                prepared_node(scene, K_LISTENER_MICROPHONE);
            const std::vector<double> power =
                power_spectrum_db(render(*node, impulse(frames)), frames);
            const double weight = static_cast<double>(poses);
            octaves[0].rendered += seam_band_power(power, frames, 0.5 * crossover, crossover) / weight;
            octaves[0].expected += modal_pose_power(*set, scene) / weight;
            octaves[1].rendered +=
                seam_band_power(power, frames, crossover, 2.0 * crossover) / weight;
            octaves[1].expected += geometric_reference_power(field, crossover, frames) / weight;
        }
        return octaves;
    }

    /// @brief Returns the modal octave's rendered level against the design's, in dB.
    double modal_level_db(const std::array<OctavePower, 2>& octaves)
    {
        return 10.0 * std::log10(octaves[0].rendered / octaves[0].expected);
    }

    /// @brief Returns the geometric octave's rendered level against the reference's, in dB.
    double geometric_level_db(const std::array<OctavePower, 2>& octaves)
    {
        return 10.0 * std::log10(octaves[1].rendered / octaves[1].expected);
    }

    /// @brief Returns the garage's octaves over the medium render, made once.
    const std::array<OctavePower, 2>& medium_garage_octaves()
    {
        static const std::array<OctavePower, 2> octaves =
            seam_octaves(SushiDSP::test::garage_scene(), K_MEDIUM_POSES, K_MEDIUM_FRAMES);
        return octaves;
    }

    /// @brief Names one of the three reference rooms.
    struct RoomRow
    {
            /// @brief Names the row.
            const char* name;

            /// @brief Builds the room.
            RoomScene (*room)();
    };

    const RoomRow K_ROOMS[] = {
        {"Garage", &SushiDSP::test::garage_scene},
        {"Bedroom", &SushiDSP::test::bedroom_scene},
        {"Block", &SushiDSP::test::block_room_scene},
    };

    /// @brief Returns the octaves of one room over the large render, made once per room.
    const std::array<OctavePower, 2>& large_octaves(const RoomRow& row)
    {
        static std::map<std::string, std::array<OctavePower, 2>> cache;
        auto found = cache.find(row.name);
        if (found == cache.end())
        {
            found = cache.emplace(row.name, seam_octaves(row.room(), K_SEAM_POSES.size(),
                                                         K_LARGE_FRAMES))
                        .first;
        }
        return found->second;
    }

    /// @brief Names a room row for the test names.
    std::string room_name(const ::testing::TestParamInfo<RoomRow>& info)
    {
        return info.param.name;
    }

    /// @brief Holds the crossover under which the node switches the wave layer off, in hertz.
    constexpr double K_LAYER_OFF_HZ = 40.0;

    /// @brief Holds the frames of the longest line of the tail, 525 ms.
    constexpr std::uint32_t K_LONGEST_LINE_FRAMES = 25200;

    /// @brief Holds the blocks of the dead-room composition render: 40 blocks, 10240 frames, past
    /// the early ring's 8397.
    constexpr std::uint32_t K_COMPOSITION_BLOCKS = 40;

    /// @brief Counts the rounded operations the node's output takes beyond the bricks rendered
    /// beside it: the tail's level, the sum and the copy to the port, with the early renderer's
    /// own accumulation over the 28 elements of the cabinet counted as the rest.
    constexpr std::size_t K_COMPOSITION_OPERATIONS = 64;

    /// @brief Renders the dead room's early bank and tail bricks beside the node, block by block,
    /// and returns channel 0 of the early renderer plus the tail.
    std::vector<float> bricks_alone(const SushiDSP::room::RoomRenderSet& set,
                                    const std::vector<float>& input)
    {
        const std::uint32_t ring_frames = SushiDSP::Room::max_early_delay_frames(K_SAMPLE_RATE);
        SushiDSP::room::EarlyReflectionRenderer early;
        early.prepare(ring_frames, K_BLOCK);
        SushiDSP::room::DelayRing early_ring;
        early_ring.prepare(ring_frames, K_BLOCK);
        SushiDSP::room::LateReverb late;
        late.prepare(K_LONGEST_LINE_FRAMES);
        std::array<std::vector<float>, SushiDSP::room::EarlyReflectionRenderer::K_OUTPUT_CHANNELS>
            channels;
        std::array<float*, SushiDSP::room::EarlyReflectionRenderer::K_OUTPUT_CHANNELS> pointers{};
        for (std::size_t channel = 0; channel < channels.size(); ++channel)
        {
            channels[channel].assign(K_BLOCK, 0.0f);
            pointers[channel] = channels[channel].data();
        }
        std::vector<float> tail(K_BLOCK);
        std::vector<float> expected(input.size(), 0.0f);
        for (std::size_t offset = 0; offset + K_BLOCK <= input.size(); offset += K_BLOCK)
        {
            const float* block_input = input.data() + offset;
            for (std::vector<float>& channel : channels)
            {
                std::fill(channel.begin(), channel.end(), 0.0f);
            }
            early_ring.write(block_input, K_BLOCK);
            early.render(early_ring, set.early, 0, pointers.data(), K_BLOCK);
            late.process(set.late, block_input, tail.data(), tail.data(), K_BLOCK);
            for (std::uint32_t frame = 0; frame < K_BLOCK; ++frame)
            {
                expected[offset + frame] = channels[0][frame] + tail[frame];
            }
        }
        return expected;
    }

    /// @brief Builds the dead room's microphone set.
    std::unique_ptr<SushiDSP::room::RoomRenderSet> dead_room_set()
    {
        const SushiDSP::room::HrtfSet hrtf = SushiDSP::room::SphericalHeadHrtf::build(K_SAMPLE_RATE);
        return SushiDSP::room::build_render_set(dead_room_scene(), hrtf, K_SAMPLE_RATE,
                                                SushiDSP::room::ListenerKind::Microphone);
    }

    /// @brief Counts the rounded operations between a stored crossover and the figure it was
    /// cast from: the cast to float, and the round down to a float the solver may take.
    constexpr std::size_t K_CROSSOVER_OPERATIONS = 2;
} // namespace

TEST(Medium_RoomNodeSeam, RoomNodeCarriesTheDesignedModalLevelInTheOctaveUnderTheCrossover)
{
    // The modal octave is held to the design's own figure at the pose.
    const double level_db = modal_level_db(medium_garage_octaves());
    RecordProperty("garage_modal_against_design_db", level_db);
    EXPECT_NEAR(level_db, 0.0, K_SEAM_BAND_DB);
}

TEST(Medium_RoomNodeSeam, RoomNodeCarriesTheDiffuseFieldLevelInTheOctaveOverTheCrossover)
{
    const double level_db = geometric_level_db(medium_garage_octaves());
    RecordProperty("garage_geometric_against_reference_db", level_db);
    EXPECT_NEAR(level_db, 0.0, K_SEAM_BAND_DB);
}

class Large_RoomNodeSeam : public ::testing::TestWithParam<RoomRow>
{
};

TEST_P(Large_RoomNodeSeam, RoomNodeCarriesTheDesignedModalLevelOverSixPoses)
{
    const double level_db = modal_level_db(large_octaves(GetParam()));
    RecordProperty("modal_against_design_db", level_db);
    EXPECT_NEAR(level_db, 0.0, K_SEAM_BAND_DB);
}

TEST_P(Large_RoomNodeSeam, RoomNodeCarriesTheDiffuseFieldLevelOverSixPoses)
{
    const double level_db = geometric_level_db(large_octaves(GetParam()));
    RecordProperty("geometric_against_reference_db", level_db);
    EXPECT_NEAR(level_db, 0.0, K_SEAM_BAND_DB);
}

INSTANTIATE_TEST_SUITE_P(Rooms, Large_RoomNodeSeam, ::testing::ValuesIn(K_ROOMS), room_name);

TEST(Medium_RoomNodeCrossover, RoomNodeReportsEyringsFigureWhenTheModesFit)
{
    // The garage's modes fit, so the report is Eyring's 278.18 Hz as the set carries it.
    const RoomScene garage = wave_scene();
    const std::unique_ptr<SushiDSP::room::RoomNode> node =
        prepared_node(garage, K_LISTENER_MICROPHONE);
    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_relative_error<float>(
        node->schroeder_frequency_hz(), SushiDSP::room::EyringDecay::schroeder_frequency_hz(garage),
        K_CROSSOVER_OPERATIONS, 1.0)));
}

TEST(Medium_RoomNodeCrossover, RoomNodeReportsTheLoweredCrossoverWhenTheModesDoNotFit)
{
    // About 1250 modes lie under the Eyring figure, so the solver lowers the crossover.
    const RoomScene small = small_concrete_room();
    const double eyring = SushiDSP::room::EyringDecay::schroeder_frequency_hz(small);
    ASSERT_GT(eyring, 800.0);
    const auto design =
        std::make_unique<SushiDSP::room::ModalDesign>(SushiDSP::room::ModalSolver::design(
            small, K_SAMPLE_RATE, eyring, SushiDSP::room::ListenerKind::Microphone));
    ASSERT_EQ(design->count, SushiDSP::room::ModalDesign::K_MAX_MODES);
    std::unique_ptr<SushiDSP::room::RoomNode> node =
        prepared_node(small, K_LISTENER_MICROPHONE);
    EXPECT_LT(node->schroeder_frequency_hz(), eyring);
    EXPECT_DOUBLE_EQ(node->schroeder_frequency_hz(), static_cast<double>(design->crossover_hz));
}

TEST(Medium_RoomNodeCrossover, RoomNodeReportsEyringsFigureOfADeadRoomWithTheLayerOff)
{
    const RoomScene dead = dead_room_scene();
    const std::unique_ptr<SushiDSP::room::RoomNode> node =
        prepared_node(dead, K_LISTENER_MICROPHONE);
    EXPECT_DOUBLE_EQ(node->schroeder_frequency_hz(),
                     SushiDSP::room::EyringDecay::schroeder_frequency_hz(dead));
    EXPECT_GT(node->schroeder_frequency_hz(), 0.0);
}

TEST(Small_RoomNodeCrossover, RoomNodeReportsEyringsFigureBeforePrepare)
{
    const RoomScene small = small_concrete_room();
    SushiDSP::room::RoomNode fresh;
    std::string reason;
    ASSERT_TRUE(publish_scene(fresh, small, reason)) << reason;
    EXPECT_DOUBLE_EQ(fresh.schroeder_frequency_hz(),
                     SushiDSP::room::EyringDecay::schroeder_frequency_hz(small));
}

TEST(Medium_RoomNodeDeadRoom, RoomNodeSwitchesTheModalLayerOffUnderTheLayerLimit)
{
    // f_s under 40 Hz gives a design with count zero and a crossover of zero.
    ASSERT_LT(SushiDSP::room::EyringDecay::schroeder_frequency_hz(dead_room_scene()),
              K_LAYER_OFF_HZ);
    const std::unique_ptr<SushiDSP::room::RoomRenderSet> set = dead_room_set();
    EXPECT_EQ(set->modal.count, 0u);
    EXPECT_EQ(set->crossover_hz, 0.0f);
}

TEST(Medium_RoomNodeDeadRoom, RoomNodeRendersTheGeometricBricksAloneWithTheLayerOff)
{
    // The node applies the tail's level after the network: rounding alone separates them.
    const std::unique_ptr<SushiDSP::room::RoomRenderSet> set = dead_room_set();
    std::unique_ptr<SushiDSP::room::RoomNode> node =
        prepared_node(dead_room_scene(), K_LISTENER_MICROPHONE);
    const std::vector<float> input = tone(K_COMPOSITION_BLOCKS * K_BLOCK);
    const std::vector<float> heard = render(*node, input);
    const std::vector<float> expected = bricks_alone(*set, input);
    const std::size_t compared = input.size() - K_ALIGN_FRAMES;
    const double peak = peak_of(expected, 0, compared);
    ASSERT_GT(peak, 0.0);
    EXPECT_LE(peak_difference(heard, expected, 0, compared),
              static_cast<double>(K_COMPOSITION_OPERATIONS) *
                  TestSupport::unit_roundoff<float>() * peak);
}
