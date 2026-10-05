/****************************************************************/
/* test_eyring_decay.cpp                                        */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file test_eyring_decay.cpp
 * @brief Tests for EyringDecay, the per-band decay time and the Schroeder frequency of a scene.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>

#include "RoomFixtures.hpp"
#include "SushiDSP/room/AirAbsorption.hpp"
#include "SushiDSP/room/EyringDecay.hpp"
#include "SushiDSP/room/RoomScene.hpp"

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
}

TEST(Unit_Dsp, EyringDecayReproducesTheGaragesSixBandDecay)
{
    // V = 60, S = 98; alpha = (92 concrete + 6 doorway) / 98 per band, m from ISO 9613-1:
    // 1.3415, 1.3326, 1.1608, 1.1460, 1.1081, 0.8931 s.
    const std::array<double, 6> expected = { 1.342, 1.333, 1.161, 1.146, 1.108, 0.893 };
    expect_bands_near(
        SushiDSP::room::EyringDecay::rt60_seconds(SushiDSP::test::garage_scene()), expected, 0.002);
}

TEST(Unit_Dsp, EyringDecayReproducesTheBedroomsSixBandDecay)
{
    // V = 36.4, S = 67; alpha = (14 carpet + 51.5 gypsum + 1.5 glass) / 67 per band:
    // 0.3264, 0.8705, 1.1542, 0.7307, 0.4281, 0.3565 s.
    const std::array<double, 6> expected = { 0.326, 0.871, 1.154, 0.731, 0.428, 0.356 };
    expect_bands_near(
        SushiDSP::room::EyringDecay::rt60_seconds(SushiDSP::test::bedroom_scene()), expected, 0.002);
}

TEST(Unit_Dsp, EyringDecayReproducesTheBlockRoomsSixBandDecay)
{
    // V = 54, S = 88.6; alpha is the painted block row itself:
    // 0.9292, 1.8860, 1.5476, 1.3051, 0.9826, 0.9809 s.
    const std::array<double, 6> expected = { 0.929, 1.886, 1.548, 1.305, 0.983, 0.981 };
    expect_bands_near(
        SushiDSP::room::EyringDecay::rt60_seconds(SushiDSP::test::block_room_scene()), expected,
        0.002);
}

TEST(Unit_Dsp, EyringDecayComputesTheSchroederFrequency)
{
    // 2000 * sqrt(RT60(500 Hz) / V): garage 2000 * sqrt(1.1608 / 60) = 278.18 Hz,
    // bedroom 2000 * sqrt(1.1542 / 36.4) = 356.14 Hz, block room 2000 * sqrt(1.5476 / 54) = 338.58 Hz.
    EXPECT_NEAR(SushiDSP::room::EyringDecay::schroeder_frequency_hz(SushiDSP::test::garage_scene()),
                278.2, 0.5);
    EXPECT_NEAR(
        SushiDSP::room::EyringDecay::schroeder_frequency_hz(SushiDSP::test::bedroom_scene()), 356.1,
        0.5);
    EXPECT_NEAR(
        SushiDSP::room::EyringDecay::schroeder_frequency_hz(SushiDSP::test::block_room_scene()),
        338.6, 0.5);
}

TEST(Unit_Dsp, EyringDecayDivergesFromSabineOnlyInAnAbsorbentRoom)
{
    // Without the air term the ratio Sabine / Eyring is exactly -ln(1 - a) / a: 1.010 at
    // a = 0.02, 1.042 at a = 0.0796 and 1.386 at a = 0.5. Both forms below keep the air term, so
    // the 125 Hz band is used: there 4 m V is 0.024 m2 against a surface term of at least 1.98 m2,
    // and the three ratios still land within 0.002.
    const double volume = 60.0;
    const double surface_area = 98.0;
    const std::array<double, 3> alpha = { 0.02, 0.0796, 0.5 };
    const std::array<double, 3> expected_ratio = { 1.010, 1.042, 1.386 };
    for (std::size_t index = 0; index < 3u; ++index)
    {
        const double sabine = sabine_seconds(volume, surface_area, alpha[index], 0u);
        const double eyring = eyring_seconds(volume, surface_area, alpha[index], 0u);
        EXPECT_NEAR(sabine / eyring, expected_ratio[index], 0.002) << "alpha " << alpha[index];
    }

    // The garage's mean absorption never exceeds 0.09, so its Sabine and Eyring decays agree
    // within 5 % in every band, which is why a concrete garage comes out the same either way.
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
    const double bedroom_ratio =
        sabine_seconds(bedroom.volume_m3(), bedroom.surface_area_m2(),
                       bedroom.mean_absorption()[5], 5u)
        / SushiDSP::room::EyringDecay::rt60_seconds(bedroom)[5];
    EXPECT_GT(bedroom_ratio, 1.10);
}

TEST(Unit_Dsp, EyringDecayHandlesAFullyAbsorbentRoom)
{
    // Every surface OpenDoorway: mean alpha is 1, -ln(0) is infinite, and the decay is 0 s in
    // every band rather than a NaN, so the Schroeder frequency is 0 Hz as well.
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

TEST(Unit_Dsp, EyringDecayOfAnUnfurnishedSceneIsUnchanged)
{
    // With nothing furnished the six values equal the air-only hand formula bit for bit.
    const SushiDSP::room::RoomScene scene;
    const std::array<double, 6> mean_alpha = scene.mean_absorption();
    const std::array<double, 6> computed = SushiDSP::room::EyringDecay::rt60_seconds(scene);
    for (std::size_t band = 0; band < 6u; ++band)
    {
        EXPECT_EQ(
            computed[band],
            eyring_seconds(scene.volume_m3(), scene.surface_area_m2(), mean_alpha[band], band))
            << "band " << band;
    }
}

TEST(Unit_Dsp, EyringDecayAddsTheFurnishingAreaToTheDenominator)
{
    using SushiDSP::room::FurnitureId;

    SushiDSP::room::RoomScene scene;
    scene.furnishings[0] = { FurnitureId::WardrobeModule, 3.0 };
    scene.furnishings[1] = { FurnitureId::SofaTwoSeat, 2.0 };
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
        const double expected = 0.161 * volume
            / (-surface_area * std::log(1.0 - mean_alpha[band]) + air + furnishing_area[band]);
        EXPECT_NEAR(computed[band], expected, 1e-9 * expected) << "band " << band;
    }
}

TEST(Unit_Dsp, EyringDecayShortensWithEveryFurnishing)
{
    using SushiDSP::room::FurnitureId;

    SushiDSP::room::RoomScene scene;
    const std::array<double, 6> bare = SushiDSP::room::EyringDecay::rt60_seconds(scene);

    scene.furnishings[0] = { FurnitureId::WardrobeModule, 3.0 };
    scene.furnishing_count = 1;
    const std::array<double, 6> wardrobed = SushiDSP::room::EyringDecay::rt60_seconds(scene);

    scene.furnishings[1] = { FurnitureId::SofaTwoSeat, 2.0 };
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
