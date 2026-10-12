/**************************************************************************/
/* test_medium_absorption.cpp                                             */
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
 * @file test_medium_absorption.cpp
 * @brief Tests for MediumAbsorption, the air and furnishing loss of one room scene.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>

#include "SushiDSP/room/air_absorption.hpp"
#include "SushiDSP/room/furniture.hpp"
#include "SushiDSP/room/medium_absorption.hpp"
#include "SushiDSP/room/room_scene.hpp"

namespace
{
    using SushiDSP::room::AirAbsorption;
    using SushiDSP::room::BandResponse;
    using SushiDSP::room::FurnitureId;
    using SushiDSP::room::K_BAND_COUNT;
    using SushiDSP::room::MediumAbsorption;
    using SushiDSP::room::RoomScene;

    /// @brief Builds the default scene with three wardrobe modules and two two-seat sofas.
    RoomScene furnished_scene()
    {
        RoomScene scene{};
        scene.furnishings[0] = RoomScene::Furnishing{FurnitureId::WardrobeModule, 3.0};
        scene.furnishings[1] = RoomScene::Furnishing{FurnitureId::SofaTwoSeat, 2.0};
        scene.furnishing_count = 2;
        return scene;
    }

    /// @brief Returns the furnishing coefficient of furnished_scene() worked out by hand, for
    /// one band.
    double expected_furnishing(std::size_t band)
    {
        const double wardrobe = static_cast<double>(
            SushiDSP::room::furniture(FurnitureId::WardrobeModule).absorption_area[band]);
        const double sofa = static_cast<double>(
            SushiDSP::room::furniture(FurnitureId::SofaTwoSeat).absorption_area[band]);
        return (3.0 * wardrobe + 2.0 * sofa) / (4.0 * 60.0);
    }

    /// @brief Expects a scene's medium to match the air's, band by band, for coefficients and
    /// bit for bit for path gains.
    void expect_matches_air(const MediumAbsorption& medium)
    {
        for (std::size_t band = 0; band < K_BAND_COUNT; ++band)
        {
            EXPECT_EQ(medium.energy_coefficient_per_metre(band),
                      AirAbsorption::energy_coefficient_per_metre(band))
                << "band " << band;
            EXPECT_EQ(medium.furnishing_coefficient_per_metre(band), 0.0) << "band " << band;
        }
        for (const double metres : {0.0, 1.0, 7.3, 30.0})
        {
            const BandResponse got = medium.over_path(metres);
            const BandResponse air = AirAbsorption::over_path(metres);
            for (std::size_t band = 0; band < K_BAND_COUNT; ++band)
            {
                EXPECT_EQ(got.gain[band], air.gain[band]) << "band " << band << " at " << metres;
            }
        }
    }
} // namespace

TEST(Unit_Dsp, MediumAbsorptionOfAnUnfurnishedSceneIsTheAirsOwn)
{
    expect_matches_air(MediumAbsorption(RoomScene{}));
}

TEST(Unit_Dsp, MediumAbsorptionTreatsACountOfZeroAsUnfurnished)
{
    RoomScene scene = furnished_scene();
    scene.furnishing_count = 0;
    expect_matches_air(MediumAbsorption(scene));
}

TEST(Unit_Dsp, MediumAbsorptionFurnishingCoefficientIsAreaOverFourVolumes)
{
    const MediumAbsorption medium(furnished_scene());
    for (std::size_t band = 0; band < K_BAND_COUNT; ++band)
    {
        EXPECT_NEAR(medium.furnishing_coefficient_per_metre(band), expected_furnishing(band), 1e-12)
            << "band " << band;
        EXPECT_GT(medium.furnishing_coefficient_per_metre(band), 0.0) << "band " << band;
    }
}

TEST(Unit_Dsp, MediumAbsorptionEnergyCoefficientIsTheAirPlusTheFurnishings)
{
    const MediumAbsorption medium(furnished_scene());
    for (std::size_t band = 0; band < K_BAND_COUNT; ++band)
    {
        EXPECT_NEAR(medium.energy_coefficient_per_metre(band),
                    AirAbsorption::energy_coefficient_per_metre(band) + expected_furnishing(band),
                    1e-12)
            << "band " << band;
    }
}

TEST(Unit_Dsp, MediumAbsorptionPathGainIsTheAirsTimesHalfTheFurnishingDecay)
{
    const MediumAbsorption medium(furnished_scene());
    for (const double metres : {2.6, 12.0})
    {
        const BandResponse got = medium.over_path(metres);
        const BandResponse air = AirAbsorption::over_path(metres);
        for (std::size_t band = 0; band < K_BAND_COUNT; ++band)
        {
            const double expected = std::exp(-0.5 * expected_furnishing(band) * metres);
            const double ratio =
                static_cast<double>(got.gain[band]) / static_cast<double>(air.gain[band]);
            EXPECT_NEAR(ratio, expected, 1e-6 * expected) << "band " << band << " at " << metres;
        }
    }
}

TEST(Unit_Dsp, MediumAbsorptionOverZeroMetresIsUnityInEveryBand)
{
    const BandResponse none = MediumAbsorption(furnished_scene()).over_path(0.0);
    for (std::size_t band = 0; band < K_BAND_COUNT; ++band)
    {
        EXPECT_FLOAT_EQ(none.gain[band], 1.0f) << "band " << band;
    }
}

TEST(Unit_Dsp, MediumAbsorptionGainNeverRisesWithDistance)
{
    const MediumAbsorption medium(furnished_scene());
    std::array<float, K_BAND_COUNT> previous = medium.over_path(0.0).gain;
    for (double metres = 0.5; metres <= 30.0; metres += 0.5)
    {
        const BandResponse current = medium.over_path(metres);
        for (std::size_t band = 0; band < K_BAND_COUNT; ++band)
        {
            EXPECT_LE(current.gain[band], previous[band]) << "band " << band << " at " << metres;
        }
        previous = current.gain;
    }
}
