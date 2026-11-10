/**************************************************************************/
/* test_bras_reference.cpp                                                */
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
 * @file test_bras_reference.cpp
 * @brief Tests for EyringDecay against the decay times measured in the BRAS CR2 seminar room.
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

    /// @brief Holds the mean T30 per band of the BRAS CR2 measurement, in seconds, as
    /// bras_cr2_reference.csv gives it. The medium test below keeps this table equal to the file.
    constexpr std::array<double, K_BAND_COUNT> K_CR2_T30_SECONDS = {1.402, 1.740, 2.022,
                                                                     1.938, 1.741, 1.563};

    /// @brief Bounds the built-room model's decay against the measurement, as a ratio each way;
    /// the model reads 1.208 at 500 Hz, so a quarter, not the Motus test's fifth. Characterised,
    /// kept from the original test.
    constexpr double K_RATIO_TOLERANCE = 0.25;

    /// @brief Bounds a figure of the CR2 model printed to one decimal: half of the last digit.
    constexpr double K_PRINTED_ONE_DECIMAL = 0.05;

    /// @brief Holds one checked band with the decay measured in it.
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

    /// @brief Reads the mean T30 per band from bras_cr2_reference.csv, in band order.
    std::vector<double> load_cr2_reference()
    {
        std::ifstream file(std::string(SUSHIDSP_FIXTURE_DIR) + "/bras_cr2_reference.csv");
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
            if (cells.size() >= 4u)
            {
                t30.push_back(std::stod(cells[3]));
            }
        }
        return t30;
    }

    /// @brief Builds a shoebox with the BRAS CR2 model's volume, total surface and area per
    /// material: 10.155 x 6.255 x 2.3 m, 146.1 m3 and 202.5 m2, with 49.29 m2 of linoleum as a hard
    /// floor covering, 108.56 m2 of concrete, 34.93 m2 of plaster and 9.75 m2 of windows placed
    /// by patches. Eyring reads only those areas and the volume, so the real plan's shape and
    /// height, which the scene's 12 m bound would refuse as a box, do not enter.
    RoomScene cr2_scene()
    {
        RoomScene scene;
        scene.dimensions = {10.155, 6.255, 2.3};
        scene.surfaces = {MaterialId::PlasterOnMasonryPainted, MaterialId::PlasterOnMasonryPainted,
                          MaterialId::ConcreteUnpainted,       MaterialId::PlasterOnMasonryPainted,
                          MaterialId::ParquetOnConcrete,       MaterialId::ConcreteUnpainted};
        scene.patches[0] = {Surface::Floor, MaterialId::ConcreteUnpainted, 14.23};
        scene.patches[1] = {Surface::PlusY, MaterialId::ConcreteUnpainted, 7.45};
        scene.patches[2] = {Surface::PlusY, MaterialId::WindowDoubleGlazed, 9.75};
        scene.patch_count = 3;
        scene.furnishing_count = 0;
        scene.source_position = {3.0, 3.0, 1.5};
        scene.listener_position = {7.0, 3.0, 1.2};
        scene.microphone_position = {7.0, 3.0, 1.2};
        return scene;
    }

    /// @brief Returns the CR2 scene with the built-room loss on.
    RoomScene cr2_built_scene()
    {
        RoomScene scene = cr2_scene();
        scene.built_room_loss = true;
        return scene;
    }

    class Small_BuiltRoomLossBras : public ::testing::TestWithParam<BandRow>
    {
    };
} // namespace

TEST(Medium_BuiltRoomLoss, BrasCr2TableEqualsTheCommittedCsv)
{
    const std::vector<double> committed = load_cr2_reference();

    ASSERT_EQ(committed.size(), K_BAND_COUNT);
    EXPECT_EQ(std::vector<double>(K_CR2_T30_SECONDS.begin(), K_CR2_T30_SECONDS.end()), committed);
}

TEST(Small_BuiltRoomLoss, BrasCr2SceneIsValidAndMatchesTheModelsVolume)
{
    const RoomScene scene = cr2_scene();
    std::string reason;

    ASSERT_TRUE(scene.is_valid(reason)) << reason;

    // 10.155 x 6.255 x 2.3 = 146.095, printed by the model as 146.1.
    EXPECT_TRUE(TestSupport::is_within(
        TestSupport::measure_error(scene.volume_m3(), 146.1, K_PRINTED_ONE_DECIMAL, 1.0)));
}

TEST(Small_BuiltRoomLoss, BrasCr2SceneMatchesTheModelsSurface)
{
    // 2 (10.155 x 6.255 + 10.155 x 2.3 + 6.255 x 2.3) = 202.525, printed by the model as 202.5.
    EXPECT_TRUE(TestSupport::is_within(
        TestSupport::measure_error(cr2_scene().surface_area_m2(), 202.5, K_PRINTED_ONE_DECIMAL, 1.0)));
}

TEST_P(Small_BuiltRoomLossBras, BrasCr2WithTheBuiltRoomLossDecaysWithinAQuarterOfMeasured)
{
    const std::size_t band = GetParam().band;
    const std::array<double, K_BAND_COUNT> built_model = EyringDecay::rt60_seconds(cr2_built_scene());

    const double ratio = built_model[band] / K_CR2_T30_SECONDS[band];

    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(ratio, 1.0, K_RATIO_TOLERANCE, 1.0)));
}

INSTANTIATE_TEST_SUITE_P(Bands, Small_BuiltRoomLossBras,
                         ::testing::Values(BandRow{"Hz500", 2u}, BandRow{"Hz1000", 3u},
                                           BandRow{"Hz2000", 4u}),
                         row_name);
