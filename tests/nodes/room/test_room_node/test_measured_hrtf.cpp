/**************************************************************************/
/* test_measured_hrtf.cpp                                                 */
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
 * @file test_measured_hrtf.cpp
 * @brief Tests for RoomNode's binaural decoder against the measured SADIE II set, which skip
 * when the set is not in the assets.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <SushiDSP/nodes/room/ambisonic_encoder.hpp>
#include <SushiDSP/nodes/room/binaural_decoder.hpp>
#include <SushiDSP/nodes/room/hrtf_set.hpp>
#include <SushiDSP/nodes/room/hrtf_set_loader.hpp>
#include <SushiDSP/nodes/room/room_node.hpp>

#include "nodes/room/test_room_node/room_node_rig.hpp"

namespace
{
    using namespace SushiDSP::test::room_node_rig;

    /// @brief Bounds the decoded near ear's third-octave error against the measured set, in dB.
    constexpr double K_NEAR_EAR_TOLERANCE_DB = 3.0;

    /// @brief Names the lowest third octave the near ear's wider bound applies from, in hertz.
    constexpr double K_NEAR_EAR_HIGH_BAND_HZ = 5000.0;

    /// @brief Bounds the decoded near ear's third-octave error from K_NEAR_EAR_HIGH_BAND_HZ up, in dB.
    constexpr double K_NEAR_EAR_HIGH_TOLERANCE_DB = 6.0;

    /// @brief Bounds the decoded far ear's third-octave error against the measured set, in dB.
    constexpr double K_FAR_EAR_TOLERANCE_DB = 10.0;

    /// @brief Bounds the rendered rear notch's depth against the set's own, in dB.
    constexpr double K_NOTCH_TOLERANCE_DB = 1.0;

    /// @brief Holds the |sin(azimuth)| under which a direction counts as in the median plane.
    constexpr double K_MEDIAN_PLANE_SINE = 0.1;

    /// @brief Holds the frames one decoded plane wave runs for.
    constexpr std::size_t K_DECODED_FRAMES = 1024;

    /// @brief Holds the third-octave centres the decoder acceptance reads, 200 Hz to 8 kHz.
    constexpr std::array<double, 17> K_DECODER_BANDS = {
        200.0,  250.0,  315.0,  400.0,  500.0,  630.0,  800.0,  1000.0, 1250.0,
        1600.0, 2000.0, 2500.0, 3150.0, 4000.0, 5000.0, 6300.0, 8000.0};

    /// @brief Returns the path of the converted SADIE II set.
    std::string measured_set_path()
    {
        return std::string(SUSHIDSP_ASSETS_DIR) + "/hrtf/sadie2_ku100_48k.hrtf";
    }

    /// @brief Holds the measured set and the decoder prepared from it, read once.
    struct MeasuredSet
    {
            /// @brief Stores whether the set was read.
            bool loaded = false;

            /// @brief Stores why it was not.
            std::string error;

            /// @brief Stores the set.
            SushiDSP::room::HrtfSet set;

            /// @brief Stores the decoder prepared from the set.
            std::unique_ptr<SushiDSP::room::BinauralDecoder> decoder;
    };

    /// @brief Returns the measured set, read once.
    MeasuredSet& measured()
    {
        static MeasuredSet made = []
        {
            MeasuredSet result;
            result.loaded = SushiDSP::room::HrtfSetLoader::load(measured_set_path(), K_SAMPLE_RATE,
                                                                result.set, result.error);
            if (result.loaded)
            {
                result.decoder = std::make_unique<SushiDSP::room::BinauralDecoder>();
                result.decoder->prepare(result.set, K_SAMPLE_RATE, K_BLOCK);
            }
            return result;
        }();
        return made;
    }

    /// @brief Skips the test of a fixture when the measured set is absent.
    class MeasuredSetTest : public ::testing::Test
    {
        protected:
            /// @brief Skips the test when the SADIE II set is not in the assets.
            void SetUp() override
            {
                if (!measured().loaded)
                {
                    GTEST_SKIP() << "no measured set at " << measured_set_path() << ": "
                                 << measured().error;
                }
            }
    };

    /// @brief Decodes one plane wave from a direction: an impulse encoded to sixteen channels,
    /// run through the decoder over frames frames, both ears returned.
    std::array<std::vector<float>, 2> decode_plane_wave(SushiDSP::room::BinauralDecoder& decoder,
                                                        double azimuth_rad, double elevation_rad,
                                                        std::size_t frames)
    {
        std::array<float, SushiDSP::room::AmbisonicEncoder::K_CHANNELS> gains{};
        SushiDSP::room::AmbisonicEncoder::encode(azimuth_rad, elevation_rad, gains);
        std::array<std::vector<float>, SushiDSP::room::AmbisonicEncoder::K_CHANNELS> channels;
        std::array<const float*, SushiDSP::room::AmbisonicEncoder::K_CHANNELS> pointers{};
        for (std::size_t channel = 0; channel < channels.size(); ++channel)
        {
            channels[channel].assign(K_BLOCK, 0.0f);
            pointers[channel] = channels[channel].data();
        }
        std::array<std::vector<float>, 2> ears = {std::vector<float>(frames, 0.0f),
                                                  std::vector<float>(frames, 0.0f)};
        std::vector<float> left(K_BLOCK);
        std::vector<float> right(K_BLOCK);
        decoder.reset();
        for (std::size_t offset = 0; offset + K_BLOCK <= frames; offset += K_BLOCK)
        {
            for (std::size_t channel = 0; channel < channels.size(); ++channel)
            {
                channels[channel][0] = offset == 0 ? gains[channel] : 0.0f;
            }
            decoder.process(pointers.data(), left.data(), right.data(), K_BLOCK);
            std::copy(left.begin(), left.end(),
                      ears[0].begin() + static_cast<std::ptrdiff_t>(offset));
            std::copy(right.begin(), right.end(),
                      ears[1].begin() + static_cast<std::ptrdiff_t>(offset));
        }
        return ears;
    }

    /// @brief Returns the bound on the third-octave error of one ear in one band.
    double tolerance_db(bool far_ear, double band_hz)
    {
        if (far_ear)
        {
            return K_FAR_EAR_TOLERANCE_DB;
        }
        return band_hz >= K_NEAR_EAR_HIGH_BAND_HZ ? K_NEAR_EAR_HIGH_TOLERANCE_DB
                                                  : K_NEAR_EAR_TOLERANCE_DB;
    }

    /// @brief Names one direction of the grid: an elevation and a step of 45 degrees of azimuth.
    struct DirectionRow
    {
            /// @brief Names the elevation for the test names.
            const char* elevation_tag;

            /// @brief Stores the elevation, in degrees.
            int elevation_deg;

            /// @brief Stores the azimuth in steps of 45 degrees.
            unsigned step;
    };

    /// @brief Returns how far the decoder's worst third octave of either ear is over its bound
    /// against the measured pair at one direction, in dB; not above zero when every one is
    /// within it.
    double worst_excess_db(const DirectionRow& row)
    {
        const double azimuth = row.step * K_PI / 4.0;
        const double elevation = row.elevation_deg * K_PI / 180.0;
        const std::array<std::vector<float>, 2> decoded =
            decode_plane_wave(*measured().decoder, azimuth, elevation, K_DECODED_FRAMES);
        const SushiDSP::room::HrtfSet::Pair pair = measured().set.interpolate(azimuth, elevation);
        double worst = -1e9;
        for (std::uint32_t ear = 0; ear < 2; ++ear)
        {
            const std::vector<double> decoded_db =
                windowed_power_db(decoded[ear], 0, decoded[ear].size());
            const std::vector<float> hrir(ear == 0 ? pair.left.begin() : pair.right.begin(),
                                          ear == 0 ? pair.left.end() : pair.right.end());
            const std::vector<double> measured_db = windowed_power_db(hrir, 0, hrir.size());
            const double side = std::sin(azimuth);
            const bool far_ear = ear == 0 ? side < -K_MEDIAN_PLANE_SINE : side > K_MEDIAN_PLANE_SINE;
            for (const double band : K_DECODER_BANDS)
            {
                worst = std::max(worst, std::fabs(third_octave_db(decoded_db, band) -
                                                  third_octave_db(measured_db, band)) -
                                            tolerance_db(far_ear, band));
            }
        }
        return worst;
    }

    /// @brief Returns the depth of the notch between the 5 and 12.5 kHz third octaves, in dB:
    /// their mean less the lowest of the 6.3, 8 and 10 kHz third octaves.
    double rear_notch_depth_db(const std::vector<double>& power_db)
    {
        double floor_db = 1e9;
        for (const double band : {6300.0, 8000.0, 10000.0})
        {
            floor_db = std::min(floor_db, third_octave_db(power_db, band));
        }
        return 0.5 * (third_octave_db(power_db, 5000.0) + third_octave_db(power_db, 12500.0)) -
               floor_db;
    }

    /// @brief Returns the grid's directions: three elevations of eight azimuths.
    std::vector<DirectionRow> direction_rows()
    {
        std::vector<DirectionRow> rows;
        const std::array<std::pair<const char*, int>, 3> elevations = {
            {{"Minus30", -30}, {"Zero", 0}, {"Plus30", 30}}};
        for (const std::pair<const char*, int>& elevation : elevations)
        {
            for (unsigned step = 0; step < 8; ++step)
            {
                rows.push_back({elevation.first, elevation.second, step});
            }
        }
        return rows;
    }

    /// @brief Names a direction row for the test names.
    std::string direction_name(const ::testing::TestParamInfo<DirectionRow>& info)
    {
        return std::string("Elevation") + info.param.elevation_tag + "Step" +
               std::string(1, static_cast<char>('0' + info.param.step));
    }
} // namespace

class Medium_RoomNodeDecoderMagnitude : public MeasuredSetTest,
                                        public ::testing::WithParamInterface<DirectionRow>
{
};

TEST_P(Medium_RoomNodeDecoderMagnitude, RoomNodeDecoderMagnitudeMatchesTheMeasuredSet)
{
    // Near ear 3 dB below 5 kHz and 6 dB above, far ear 10 (assets/hrtf/README.md).
    EXPECT_LE(worst_excess_db(GetParam()), 0.0);
}

INSTANTIATE_TEST_SUITE_P(Directions, Medium_RoomNodeDecoderMagnitude,
                         ::testing::ValuesIn(direction_rows()), direction_name);

class Medium_RoomNodeRearNotch : public MeasuredSetTest
{
};

TEST_F(Medium_RoomNodeRearNotch, RoomNodeCarriesTheRearNotchWithAMeasuredSet)
{
    // The ears facing away from the cabinet hear the set's own rear notch (assets/hrtf/README.md).
    const SushiDSP::room::HrtfSet::Pair rear = measured().set.interpolate(K_PI, 0.0);
    const std::vector<float> rear_left(rear.left.begin(), rear.left.end());
    const double measured_depth_db =
        rear_notch_depth_db(windowed_power_db(rear_left, 0, rear_left.size()));
    std::unique_ptr<SushiDSP::room::RoomNode> node =
        prepared_node(centred_dead_room(), K_LISTENER_EARS);
    SushiDSP::room::HrtfSet installed = measured().set;
    ASSERT_EQ(node->set_hrtf(std::move(installed)), SushiDSP::room::HrtfStatus::Installed);
    ASSERT_EQ(node->hrtf_source(), SushiDSP::room::HrtfSource::Measured);
    const double rendered_depth_db = rear_notch_depth_db(windowed_power_db(
        render(*node, impulse(K_DIRECT_WINDOW_BLOCKS * K_BLOCK)), 0, K_DIRECT_WINDOW));
    RecordProperty("notch_depth_db", rendered_depth_db);
    EXPECT_NEAR(rendered_depth_db, measured_depth_db, K_NOTCH_TOLERANCE_DB);
}
