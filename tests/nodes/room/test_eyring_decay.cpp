/**************************************************************************/
/* test_eyring_decay.cpp                                                  */
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
 * @file test_eyring_decay.cpp
 * @brief Tests for EyringDecay, the per-band decay time and the Schroeder frequency of a scene.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>

#include <SushiDSP/nodes/room_scenes/room_fixtures.hpp>
#include <SushiDSP/nodes/room/air_absorption.hpp>
#include <SushiDSP/nodes/room/eyring_decay.hpp>
#include <SushiDSP/nodes/room/room_scene.hpp>

namespace
{
    /// @brief Compares six decay times against a reference table within a tolerance in seconds.
    void expect_bands_near(const std::array<double, 6>& computed,
                           const std::array<double, 6>& expected, double tolerance)
    {
        for (std::size_t band = 0; band < 6u; ++band)
        {
            EXPECT_NEAR(computed[band], expected[band], tolerance) << "band " << band;
        }
    }

    /// @brief Evaluates Sabine's linear form 0.161 V / (S alpha + 4 m V) for one band.
    double sabine_seconds(double volume, double surface_area, double alpha, std::size_t band)
    {
        const double air =
            4.0 * SushiDSP::room::AirAbsorption::energy_coefficient_per_metre(band) * volume;
        return 0.161 * volume / (surface_area * alpha + air);
    }

    /// @brief Evaluates Eyring's form 0.161 V / (-S ln(1 - alpha) + 4 m V) for one band.
    double eyring_seconds(double volume, double surface_area, double alpha, std::size_t band)
    {
        const double air =
            4.0 * SushiDSP::room::AirAbsorption::energy_coefficient_per_metre(band) * volume;
        return 0.161 * volume / (-surface_area * std::log(1.0 - alpha) + air);
    }
} // namespace

TEST(EyringDecay, EyringDecayReproducesTheGaragesSixBandDecay)
{
    // V = 60 and S = 98 give these six decay times. See tests/nodes/room/README.md.
    const std::array<double, 6> expected = {1.342, 1.333, 1.161, 1.146, 1.108, 0.893};
    expect_bands_near(SushiDSP::room::EyringDecay::rt60_seconds(SushiDSP::test::garage_scene()),
                      expected, 0.002);
}

TEST(EyringDecay, EyringDecayReproducesTheBedroomsSixBandDecay)
{
    // V = 36.4 and S = 67 give these six decay times. See tests/nodes/room/README.md.
    const std::array<double, 6> expected = {0.326, 0.871, 1.154, 0.731, 0.428, 0.356};
    expect_bands_near(SushiDSP::room::EyringDecay::rt60_seconds(SushiDSP::test::bedroom_scene()),
                      expected, 0.002);
}

TEST(EyringDecay, EyringDecayReproducesTheBlockRoomsSixBandDecay)
{
    // V = 54 and S = 88.6 give these six decay times. See tests/nodes/room/README.md.
    const std::array<double, 6> expected = {0.929, 1.886, 1.548, 1.305, 0.983, 0.981};
    expect_bands_near(SushiDSP::room::EyringDecay::rt60_seconds(SushiDSP::test::block_room_scene()),
                      expected, 0.002);
}

TEST(EyringDecay, EyringDecayComputesTheSchroederFrequency)
{
    // Schroeder frequency = 2000 * sqrt(RT60(500 Hz) / V). See tests/nodes/room/README.md.
    EXPECT_NEAR(SushiDSP::room::EyringDecay::schroeder_frequency_hz(SushiDSP::test::garage_scene()),
                278.2, 0.5);
    EXPECT_NEAR(
        SushiDSP::room::EyringDecay::schroeder_frequency_hz(SushiDSP::test::bedroom_scene()), 356.1,
        0.5);
    EXPECT_NEAR(
        SushiDSP::room::EyringDecay::schroeder_frequency_hz(SushiDSP::test::block_room_scene()),
        338.6, 0.5);
}

TEST(EyringDecay, EyringDecayDivergesFromSabineOnlyInAnAbsorbentRoom)
{
    // Without air, Sabine / Eyring is -ln(1 - a) / a. See tests/nodes/room/README.md.
    const double volume = 60.0;
    const double surface_area = 98.0;
    const std::array<double, 3> alpha = {0.02, 0.0796, 0.5};
    const std::array<double, 3> expected_ratio = {1.010, 1.042, 1.386};
    for (std::size_t index = 0; index < 3u; ++index)
    {
        const double sabine = sabine_seconds(volume, surface_area, alpha[index], 0u);
        const double eyring = eyring_seconds(volume, surface_area, alpha[index], 0u);
        EXPECT_NEAR(sabine / eyring, expected_ratio[index], 0.002) << "alpha " << alpha[index];
    }

    // The garage's mean absorption never exceeds 0.09. See tests/nodes/room/README.md.
    const SushiDSP::room::RoomScene garage = SushiDSP::test::garage_scene();
    const std::array<double, 6> mean_alpha = garage.mean_absorption();
    const std::array<double, 6> computed = SushiDSP::room::EyringDecay::rt60_seconds(garage);
    for (std::size_t band = 0; band < 6u; ++band)
    {
        const double volume_m3 = garage.volume_m3();
        const double surface_m2 = garage.surface_area_m2();
        EXPECT_NEAR(computed[band], eyring_seconds(volume_m3, surface_m2, mean_alpha[band], band),
                    1e-12)
            << "band " << band;
        const double ratio =
            sabine_seconds(volume_m3, surface_m2, mean_alpha[band], band) / computed[band];
        EXPECT_GT(ratio, 1.0) << "band " << band;
        EXPECT_LT(ratio, 1.05) << "band " << band;
    }

    // The bedroom's 4 kHz band sits at alpha = 0.206, where Sabine overshoots Eyring by 12 %.
    const SushiDSP::room::RoomScene bedroom = SushiDSP::test::bedroom_scene();
    const double bedroom_ratio = sabine_seconds(bedroom.volume_m3(), bedroom.surface_area_m2(),
                                                bedroom.mean_absorption()[5], 5u) /
                                 SushiDSP::room::EyringDecay::rt60_seconds(bedroom)[5];
    EXPECT_GT(bedroom_ratio, 1.10);
}

TEST(EyringDecay, EyringDecayHandlesAFullyAbsorbentRoom)
{
    // A mean alpha of 1 gives 0 s, not a NaN. See tests/nodes/room/README.md.
    SushiDSP::room::RoomScene scene = SushiDSP::test::block_room_scene();
    scene.surfaces.fill(SushiDSP::room::MaterialId::OpenDoorway);
    const std::array<double, 6> computed = SushiDSP::room::EyringDecay::rt60_seconds(scene);
    for (std::size_t band = 0; band < 6u; ++band)
    {
        EXPECT_FALSE(std::isnan(computed[band])) << "band " << band;
        EXPECT_DOUBLE_EQ(computed[band], 0.0) << "band " << band;
    }
    EXPECT_DOUBLE_EQ(SushiDSP::room::EyringDecay::schroeder_frequency_hz(scene), 0.0);
}

TEST(EyringDecay, EyringDecayOfAnUnfurnishedSceneIsUnchanged)
{
    // With nothing furnished the six values equal the air-only hand formula bit for bit.
    const SushiDSP::room::RoomScene scene;
    const std::array<double, 6> mean_alpha = scene.mean_absorption();
    const std::array<double, 6> computed = SushiDSP::room::EyringDecay::rt60_seconds(scene);
    for (std::size_t band = 0; band < 6u; ++band)
    {
        EXPECT_EQ(computed[band], eyring_seconds(scene.volume_m3(), scene.surface_area_m2(),
                                                 mean_alpha[band], band))
            << "band " << band;
    }
}

TEST(EyringDecay, EyringDecayAddsTheFurnishingAreaToTheDenominator)
{
    using SushiDSP::room::FurnitureId;

    SushiDSP::room::RoomScene scene;
    scene.furnishings[0] = {FurnitureId::WardrobeModule, 3.0};
    scene.furnishings[1] = {FurnitureId::SofaTwoSeat, 2.0};
    scene.furnishing_count = 2;

    const double volume = scene.volume_m3();
    const double surface_area = scene.surface_area_m2();
    const std::array<double, 6> mean_alpha = scene.mean_absorption();
    const std::array<double, 6> furnishing_area = scene.furnishing_absorption_area();
    const std::array<double, 6> computed = SushiDSP::room::EyringDecay::rt60_seconds(scene);
    for (std::size_t band = 0; band < 6u; ++band)
    {
        const double air =
            4.0 * SushiDSP::room::AirAbsorption::energy_coefficient_per_metre(band) * volume;
        const double expected =
            0.161 * volume /
            (-surface_area * std::log(1.0 - mean_alpha[band]) + air + furnishing_area[band]);
        EXPECT_NEAR(computed[band], expected, 1e-9 * expected) << "band " << band;
    }
}

TEST(EyringDecay, EyringDecayShortensWithEveryFurnishing)
{
    using SushiDSP::room::FurnitureId;

    SushiDSP::room::RoomScene scene;
    const std::array<double, 6> bare = SushiDSP::room::EyringDecay::rt60_seconds(scene);

    scene.furnishings[0] = {FurnitureId::WardrobeModule, 3.0};
    scene.furnishing_count = 1;
    const std::array<double, 6> wardrobed = SushiDSP::room::EyringDecay::rt60_seconds(scene);

    scene.furnishings[1] = {FurnitureId::SofaTwoSeat, 2.0};
    scene.furnishing_count = 2;
    const std::array<double, 6> furnished = SushiDSP::room::EyringDecay::rt60_seconds(scene);

    scene.furnishings[0].quantity = 6.0;
    scene.furnishings[1].quantity = 4.0;
    const std::array<double, 6> doubled = SushiDSP::room::EyringDecay::rt60_seconds(scene);

    for (std::size_t band = 0; band < 6u; ++band)
    {
        EXPECT_LT(wardrobed[band], bare[band]) << "band " << band;
        EXPECT_LT(furnished[band], wardrobed[band]) << "band " << band;
        EXPECT_LT(doubled[band], furnished[band]) << "band " << band;
    }
}
