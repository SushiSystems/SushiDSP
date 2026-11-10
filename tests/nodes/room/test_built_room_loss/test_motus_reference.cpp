/**************************************************************************/
/* test_motus_reference.cpp                                               */
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
 * @file test_motus_reference.cpp
 * @brief Tests for EyringDecay against the decay times measured in the Motus concrete room.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <SushiDSP/nodes/room/eyring_decay.hpp>
#include <SushiDSP/nodes/room/room_scene.hpp>

#include <support/tolerance.hpp>

namespace
{
    using SushiDSP::room::EyringDecay;
    using SushiDSP::room::K_BAND_COUNT;
    using SushiDSP::room::MaterialId;
    using SushiDSP::room::RoomScene;
    using SushiDSP::room::Surface;

    /// @brief Holds the T30 of measurement 0001, the empty room, per band in seconds, as
    /// motus_reference.csv gives it. The medium test below keeps this table equal to the file.
    constexpr std::array<double, K_BAND_COUNT> K_EMPTY_ROOM_T30_SECONDS = {2.572, 2.147, 2.009,
                                                                            2.119, 1.958, 1.508};

    /// @brief Bounds the built-room model's decay against the measurement, as a ratio each way.
    /// Characterised, kept from the original test.
    constexpr double K_BUILT_RATIO_TOLERANCE = 0.2;

    /// @brief Gives the least ratio of the plain model to the measurement: the model is never
    /// shorter than the room it describes. The module's documented state; kept from the original.
    constexpr double K_PLAIN_RATIO_FLOOR = 1.0;

    /// @brief Gives the greatest ratio of the plain model to the measurement: a first hand
    /// estimate put the empty room 40 to 70 percent too long in the mid bands, so two times is a
    /// ceiling on that error, not a target.
    constexpr double K_PLAIN_RATIO_CEILING = 2.0;

    /// @brief Holds one checked band.
    struct BandRow
    {
            const char* name;
            std::size_t band;
    };

    /// @brief Names a row for gtest.
    std::string row_name(const ::testing::TestParamInfo<BandRow>& info)
    {
        return info.param.name;
    }

    /// @brief Reads the T30 per band of measurement 0001 from motus_reference.csv, in band order.
    std::vector<double> load_empty_room_t30()
    {
        std::ifstream file(std::string(SUSHIDSP_FIXTURE_DIR) + "/motus_reference.csv");
        std::vector<double> t30;
        std::string line;
        std::getline(file, line);
        while (std::getline(file, line))
        {
            std::vector<std::string> cells;
            std::stringstream stream(line);
            std::string cell;
            while (std::getline(stream, cell, ','))
            {
                cells.push_back(cell);
            }
            if (cells.size() >= 5u && cells[0] == "0001")
            {
                t30.push_back(std::stod(cells[4]));
            }
        }
        return t30;
    }

    /// @brief Builds the empty Motus room: 4.9 x 4.4 x 2.9 m of unpainted concrete with the two
    /// windows on +y and the door on -x, poses at the dataset's receiver and source.
    RoomScene motus_empty_scene()
    {
        RoomScene scene;
        scene.dimensions = {4.9, 4.4, 2.9};
        scene.surfaces.fill(MaterialId::ConcreteUnpainted);
        scene.scattering.fill(SushiDSP::room::Scattering::Flat);
        scene.patches[0] = {Surface::PlusY, MaterialId::GlassWindow, 5.53};
        scene.patches[1] = {Surface::MinusX, MaterialId::WoodenDoor, 2.27};
        scene.patch_count = 2;
        scene.furnishing_count = 0;
        scene.source_position = {3.462, 2.075, 1.448};
        scene.listener_position = {1.825, 2.075, 1.448};
        scene.microphone_position = {1.825, 2.075, 1.448};
        return scene;
    }

    /// @brief Builds the Motus room of measurement 0002: the empty room with 14.0 m2 of thin
    /// carpet on the floor.
    RoomScene motus_carpet_scene()
    {
        RoomScene scene = motus_empty_scene();
        scene.patches[scene.patch_count] = {Surface::Floor, MaterialId::ThinCarpetOnConcrete, 14.0};
        ++scene.patch_count;
        return scene;
    }

    /// @brief Builds the empty Motus room with the built-room loss on.
    RoomScene motus_built_scene()
    {
        RoomScene scene = motus_empty_scene();
        scene.built_room_loss = true;
        return scene;
    }

    class Small_BuiltRoomLossMotusPlain : public ::testing::TestWithParam<BandRow>
    {
    };

    class Small_BuiltRoomLossMotusCarpet : public ::testing::TestWithParam<BandRow>
    {
    };

    class Small_BuiltRoomLossMotusBuilt : public ::testing::TestWithParam<BandRow>
    {
    };
} // namespace

TEST(Medium_BuiltRoomLoss, MotusEmptyRoomTableEqualsTheCommittedCsv)
{
    const std::vector<double> committed = load_empty_room_t30();

    ASSERT_EQ(committed.size(), K_BAND_COUNT);
    EXPECT_EQ(std::vector<double>(K_EMPTY_ROOM_T30_SECONDS.begin(), K_EMPTY_ROOM_T30_SECONDS.end()),
              committed);
}

TEST_P(Small_BuiltRoomLossMotusPlain, MotusEmptyRoomDecayIsNotShorterThanMeasured)
{
    const std::size_t band = GetParam().band;
    const RoomScene empty = motus_empty_scene();
    std::string reason;
    ASSERT_TRUE(empty.is_valid(reason)) << reason;

    const double ratio = EyringDecay::rt60_seconds(empty)[band] / K_EMPTY_ROOM_T30_SECONDS[band];

    EXPECT_GE(ratio, K_PLAIN_RATIO_FLOOR);
}

TEST_P(Small_BuiltRoomLossMotusPlain, MotusEmptyRoomDecayIsAtMostTwiceTheMeasured)
{
    const std::size_t band = GetParam().band;

    // Two times is a ceiling on the error, not a target. See tests/nodes/room/test_built_room_loss/README.md.
    const double ratio =
        EyringDecay::rt60_seconds(motus_empty_scene())[band] / K_EMPTY_ROOM_T30_SECONDS[band];

    EXPECT_LE(ratio, K_PLAIN_RATIO_CEILING);
}

INSTANTIATE_TEST_SUITE_P(Bands, Small_BuiltRoomLossMotusPlain,
                         ::testing::Values(BandRow{"Hz250", 1u}, BandRow{"Hz500", 2u},
                                           BandRow{"Hz1000", 3u}, BandRow{"Hz2000", 4u}),
                         row_name);

TEST_P(Small_BuiltRoomLossMotusCarpet, MotusCarpetShortensTheModel)
{
    const std::size_t band = GetParam().band;
    const RoomScene carpet = motus_carpet_scene();
    std::string reason;
    ASSERT_TRUE(carpet.is_valid(reason)) << reason;

    const double carpet_model = EyringDecay::rt60_seconds(carpet)[band];
    const double empty_model = EyringDecay::rt60_seconds(motus_empty_scene())[band];

    EXPECT_LT(carpet_model, empty_model);
}

INSTANTIATE_TEST_SUITE_P(Bands, Small_BuiltRoomLossMotusCarpet,
                         ::testing::Values(BandRow{"Hz1000", 3u}, BandRow{"Hz2000", 4u},
                                           BandRow{"Hz4000", 5u}),
                         row_name);

TEST_P(Small_BuiltRoomLossMotusBuilt, MotusEmptyRoomWithTheBuiltRoomLossDecaysWithinAFifthOfMeasured)
{
    const std::size_t band = GetParam().band;
    const RoomScene scene = motus_built_scene();
    std::string reason;
    ASSERT_TRUE(scene.is_valid(reason)) << reason;

    const double ratio = EyringDecay::rt60_seconds(scene)[band] / K_EMPTY_ROOM_T30_SECONDS[band];

    EXPECT_TRUE(
        TestSupport::is_within(TestSupport::measure_error(ratio, 1.0, K_BUILT_RATIO_TOLERANCE, 1.0)));
}

INSTANTIATE_TEST_SUITE_P(Bands, Small_BuiltRoomLossMotusBuilt,
                         ::testing::Values(BandRow{"Hz500", 2u}, BandRow{"Hz1000", 3u},
                                           BandRow{"Hz2000", 4u}),
                         row_name);
