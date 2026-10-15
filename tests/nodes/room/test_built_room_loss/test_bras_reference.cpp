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
 * @file test_bras_reference.cpp
 * @brief Tests for EyringDecay against the decay times measured in the BRAS CR2 seminar room.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <SushiDSP/nodes/room/eyring_decay.hpp>
#include <SushiDSP/nodes/room/room_scene.hpp>

namespace
{
    using SushiDSP::room::EyringDecay;
    using SushiDSP::room::K_BAND_CENTRES_HZ;
    using SushiDSP::room::K_BAND_COUNT;
    using SushiDSP::room::MaterialId;
    using SushiDSP::room::RoomScene;
    using SushiDSP::room::Surface;

    /// @brief Names the first band of the built-room check, 500 Hz, as an index into the bands.
    constexpr std::size_t K_FIRST_CHECKED_BAND = 2;

    /// @brief Names the last band of the built-room check, 2 kHz, as an index into the bands.
    constexpr std::size_t K_LAST_CHECKED_BAND = 4;

    /// @brief Bounds the built-room model's decay against the measurement, as a ratio each way;
    /// the model reads 1.208 at 500 Hz, so a quarter, not the Motus test's fifth.
    constexpr double K_RATIO_TOLERANCE = 0.25;

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

    /// @brief Prints one band's measured and modelled decay and their ratio.
    void print_band(const char* label, double band_hz, double measured, double model)
    {
        std::cout << std::fixed << std::setprecision(3) << "bras cr2 " << label << "  "
                  << std::setw(5) << static_cast<int>(band_hz) << " Hz  measured T30 "
                  << std::setw(6) << measured << " s  model " << std::setw(6) << model
                  << " s  ratio " << std::setw(5) << model / measured << "\n";
    }
} // namespace

TEST(Medium_BuiltRoomLoss, BrasCr2ReferenceParsesToSixPositiveBands)
{
    const std::vector<double> measured = load_cr2_reference();
    ASSERT_EQ(measured.size(), K_BAND_COUNT);
    for (const double value : measured)
    {
        EXPECT_GT(value, 0.0);
    }
}

TEST(Medium_BuiltRoomLoss, BrasCr2SceneMatchesTheModelsVolumeAndSurface)
{
    const RoomScene scene = cr2_scene();
    std::string reason;
    ASSERT_TRUE(scene.is_valid(reason)) << reason;
    EXPECT_NEAR(scene.volume_m3(), 146.1, 0.2);
    EXPECT_NEAR(scene.surface_area_m2(), 202.53, 0.5);
}

TEST(Medium_BuiltRoomLoss, BrasCr2WithTheBuiltRoomLossDecaysWithinAQuarterOfMeasured)
{
    const std::vector<double> measured = load_cr2_reference();
    ASSERT_EQ(measured.size(), K_BAND_COUNT);
    const RoomScene tabulated = cr2_scene();
    RoomScene built = tabulated;
    built.built_room_loss = true;

    const std::array<double, K_BAND_COUNT> plain_model = EyringDecay::rt60_seconds(tabulated);
    const std::array<double, K_BAND_COUNT> built_model = EyringDecay::rt60_seconds(built);
    for (std::size_t band = 0; band < K_BAND_COUNT; ++band)
    {
        print_band("tabulated", K_BAND_CENTRES_HZ[band], measured[band], plain_model[band]);
        print_band("built    ", K_BAND_CENTRES_HZ[band], measured[band], built_model[band]);
    }
    for (std::size_t band = K_FIRST_CHECKED_BAND; band <= K_LAST_CHECKED_BAND; ++band)
    {
        const double ratio = built_model[band] / measured[band];
        EXPECT_GE(ratio, 1.0 - K_RATIO_TOLERANCE) << K_BAND_CENTRES_HZ[band] << " Hz";
        EXPECT_LE(ratio, 1.0 + K_RATIO_TOLERANCE) << K_BAND_CENTRES_HZ[band] << " Hz";
    }
}
