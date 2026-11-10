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
 * @file test_eyring_decay.cpp
 * @brief Tests for EyringDecay, the per-band decay time and the Schroeder frequency of a scene.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <string>

#include <SushiDSP/nodes/room_scenes/room_fixtures.hpp>
#include <SushiDSP/nodes/room/air_absorption.hpp>
#include <SushiDSP/nodes/room/eyring_decay.hpp>
#include <SushiDSP/nodes/room/room_scene.hpp>

#include <support/tolerance.hpp>

namespace
{
    using SushiDSP::room::EyringDecay;
    using SushiDSP::room::RoomScene;
    using Bands = std::array<double, SushiDSP::room::K_BAND_COUNT>;

    /// @brief Builds one of the reference rooms.
    using SceneBuilder = RoomScene (*)();

    /// @brief Characterised band, in seconds: the six decay times of a reference room agree with
    /// the three-decimal tables below to this. Kept from the original test.
    constexpr double K_DECAY_BAND_SECONDS = 0.002;

    /// @brief Characterised band, in hertz, of the Schroeder frequencies. Kept from the original
    /// test.
    constexpr double K_SCHROEDER_BAND_HZ = 0.5;

    /// @brief Counts the double roundings of the closed form below: the logarithm, the product,
    /// the sums and the division, with room for the air term.
    constexpr std::size_t K_CLOSED_FORM_OPERATIONS = 10;

    /// @brief Holds one reference room with the six decay times it must give.
    struct DecayRow
    {
            const char* name;
            SceneBuilder scene;
            Bands seconds;
    };

    /// @brief Holds one reference room with its Schroeder frequency.
    struct SchroederRow
    {
            const char* name;
            SceneBuilder scene;
            double hertz;
    };

    /// @brief Holds one reference room.
    struct RoomRow
    {
            const char* name;
            SceneBuilder scene;
    };

    /// @brief Holds one band index.
    struct BandRow
    {
            const char* name;
            std::size_t band;
    };

    /// @brief Names a row for gtest.
    template <typename Row>
    std::string row_name(const ::testing::TestParamInfo<Row>& info)
    {
        return info.param.name;
    }

    /// @brief Returns 4 m V, the air term of the denominator, for one band.
    double air_term(double volume, std::size_t band)
    {
        return 4.0 * SushiDSP::room::AirAbsorption::energy_coefficient_per_metre(band) * volume;
    }

    /// @brief Evaluates Sabine's linear form 0.161 V / (S alpha + 4 m V) for one band.
    double sabine_seconds(double volume, double surface_area, double alpha, std::size_t band)
    {
        return 0.161 * volume / (surface_area * alpha + air_term(volume, band));
    }

    /// @brief Evaluates Eyring's form 0.161 V / (-S ln(1 - alpha) + 4 m V) for one band.
    double eyring_seconds(double volume, double surface_area, double alpha, std::size_t band)
    {
        return 0.161 * volume / (-surface_area * std::log(1.0 - alpha) + air_term(volume, band));
    }

    /// @brief Expects six decay times to match a reference table within a band in seconds.
    void expect_bands_near(const Bands& computed, const Bands& expected, double band_seconds)
    {
        for (std::size_t band = 0; band < computed.size(); ++band)
        {
            EXPECT_TRUE(TestSupport::is_within(
                TestSupport::measure_error(computed[band], expected[band], band_seconds, 1.0)))
                << "band " << band;
        }
    }

    /// @brief Expects every band of @p computed to match the closed-form Eyring decay of @p scene
    /// to the roundings of its evaluation.
    void expect_matches_the_closed_form(const Bands& computed, const RoomScene& scene)
    {
        const Bands mean_alpha = scene.mean_absorption();
        for (std::size_t band = 0; band < computed.size(); ++band)
        {
            const double expected = eyring_seconds(scene.volume_m3(), scene.surface_area_m2(), mean_alpha[band], band);
            EXPECT_TRUE(TestSupport::is_within(
                TestSupport::measure_relative_error<double>(computed[band], expected, K_CLOSED_FORM_OPERATIONS)))
                << "band " << band;
        }
    }

    /// @brief Expects every band of @p computed to equal the air-only Eyring decay of @p scene
    /// bit for bit.
    void expect_bands_equal_the_closed_form(const Bands& computed, const RoomScene& scene)
    {
        const Bands mean_alpha = scene.mean_absorption();
        for (std::size_t band = 0; band < computed.size(); ++band)
        {
            EXPECT_EQ(computed[band],
                      eyring_seconds(scene.volume_m3(), scene.surface_area_m2(), mean_alpha[band], band))
                << "band " << band;
        }
    }

    /// @brief Expects every band of @p computed to be a number and exactly zero.
    void expect_every_band_zero(const Bands& computed)
    {
        for (std::size_t band = 0; band < computed.size(); ++band)
        {
            EXPECT_FALSE(std::isnan(computed[band])) << "band " << band;
            EXPECT_EQ(computed[band], 0.0) << "band " << band;
        }
    }

    /// @brief Expects every band of @p smaller to be below the same band of @p larger.
    void expect_every_band_below(const Bands& smaller, const Bands& larger)
    {
        for (std::size_t band = 0; band < smaller.size(); ++band)
        {
            EXPECT_LT(smaller[band], larger[band]) << "band " << band;
        }
    }

    class Small_EyringDecayRoom : public ::testing::TestWithParam<DecayRow>
    {
    };

    class Small_EyringDecaySchroeder : public ::testing::TestWithParam<SchroederRow>
    {
    };

    class Small_EyringDecaySabine : public ::testing::TestWithParam<RoomRow>
    {
    };

    class Small_EyringDecayGarage : public ::testing::TestWithParam<BandRow>
    {
    };
} // namespace

TEST_P(Small_EyringDecayRoom, EyringDecayReproducesTheRoomsSixBandDecay)
{
    // The garage is V = 60 and S = 98, the bedroom V = 36.4, S = 67 and the block room V = 54, S = 88.6.
    const DecayRow row = GetParam();

    expect_bands_near(EyringDecay::rt60_seconds(row.scene()), row.seconds, K_DECAY_BAND_SECONDS);
}

INSTANTIATE_TEST_SUITE_P(
    Rooms, Small_EyringDecayRoom,
    ::testing::Values(
        DecayRow{"Garage", SushiDSP::test::garage_scene, {1.342, 1.333, 1.161, 1.146, 1.108, 0.893}},
        DecayRow{"Bedroom", SushiDSP::test::bedroom_scene, {0.326, 0.871, 1.154, 0.731, 0.428, 0.356}},
        DecayRow{"BlockRoom", SushiDSP::test::block_room_scene, {0.929, 1.886, 1.548, 1.305, 0.983, 0.981}}),
    row_name<DecayRow>);

TEST_P(Small_EyringDecaySchroeder, EyringDecayComputesTheSchroederFrequency)
{
    // Schroeder frequency = 2000 * sqrt(RT60(500 Hz) / V). See tests/nodes/room/README.md.
    const SchroederRow row = GetParam();

    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(
        EyringDecay::schroeder_frequency_hz(row.scene()), row.hertz, K_SCHROEDER_BAND_HZ, 1.0)));
}

INSTANTIATE_TEST_SUITE_P(Rooms, Small_EyringDecaySchroeder,
                         ::testing::Values(SchroederRow{"Garage", SushiDSP::test::garage_scene, 278.2},
                                           SchroederRow{"Bedroom", SushiDSP::test::bedroom_scene, 356.1},
                                           SchroederRow{"BlockRoom", SushiDSP::test::block_room_scene, 338.6}),
                         row_name<SchroederRow>);

TEST_P(Small_EyringDecaySabine, EyringDecayDivergesFromSabineByMinusLogOfOneMinusAlphaOverAlpha)
{
    // Without air, Sabine / Eyring is -ln(1 - a) / a. See tests/nodes/room/README.md.
    const RoomScene scene = GetParam().scene();
    const double alpha = scene.mean_absorption()[0];
    const double surface = alpha * scene.surface_area_m2();
    const double ratio = sabine_seconds(scene.volume_m3(), scene.surface_area_m2(), alpha, 0u) /
                         EyringDecay::rt60_seconds(scene)[0];

    /* The air term A moves the ratio (E + A) / (s + A) from E / s by A (E - s) / (s (s + A)), under
       A (E / s - 1) / s, where s = S alpha and E = -S ln(1 - alpha). */
    const double without_air = -std::log(1.0 - alpha) / alpha;
    const double bound = air_term(scene.volume_m3(), 0u) * (without_air - 1.0) / surface +
                         static_cast<double>(K_CLOSED_FORM_OPERATIONS) * TestSupport::unit_roundoff<double>() * without_air;
    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(ratio, without_air, bound, 1.0)));
}

INSTANTIATE_TEST_SUITE_P(Rooms, Small_EyringDecaySabine,
                         ::testing::Values(RoomRow{"Garage", SushiDSP::test::garage_scene},
                                           RoomRow{"Bedroom", SushiDSP::test::bedroom_scene},
                                           RoomRow{"BlockRoom", SushiDSP::test::block_room_scene}),
                         row_name<RoomRow>);

TEST(Small_EyringDecay, EyringDecayOfTheGarageIsEyringsClosedForm)
{
    const RoomScene garage = SushiDSP::test::garage_scene();

    expect_matches_the_closed_form(EyringDecay::rt60_seconds(garage), garage);
}

TEST_P(Small_EyringDecayGarage, EyringDecayAgreesWithSabineWithinFivePercentInAConcreteGarage)
{
    // The garage's mean absorption never exceeds 0.09. See tests/nodes/room/README.md.
    const std::size_t band = GetParam().band;
    const RoomScene garage = SushiDSP::test::garage_scene();
    const double sabine = sabine_seconds(garage.volume_m3(), garage.surface_area_m2(), garage.mean_absorption()[band], band);

    const double ratio = sabine / EyringDecay::rt60_seconds(garage)[band];

    EXPECT_GT(ratio, 1.0);
    EXPECT_LT(ratio, 1.05);
}

INSTANTIATE_TEST_SUITE_P(Bands, Small_EyringDecayGarage,
                         ::testing::Values(BandRow{"Hz125", 0u}, BandRow{"Hz250", 1u},
                                           BandRow{"Hz500", 2u}, BandRow{"Hz1000", 3u},
                                           BandRow{"Hz2000", 4u}, BandRow{"Hz4000", 5u}),
                         row_name<BandRow>);

TEST(Small_EyringDecay, EyringDecayOfTheBedroomIsOverTenPercentShorterThanSabineAtFourKilohertz)
{
    // The bedroom's 4 kHz band sits at alpha = 0.206, where Sabine overshoots Eyring by 12 %.
    const RoomScene bedroom = SushiDSP::test::bedroom_scene();
    const double sabine = sabine_seconds(bedroom.volume_m3(), bedroom.surface_area_m2(), bedroom.mean_absorption()[5], 5u);

    EXPECT_GT(sabine / EyringDecay::rt60_seconds(bedroom)[5], 1.10);
}

TEST(Small_EyringDecay, EyringDecayHandlesAFullyAbsorbentRoomWithZeroDecay)
{
    // A mean alpha of 1 gives 0 s, not a NaN. See tests/nodes/room/README.md.
    RoomScene scene = SushiDSP::test::block_room_scene();
    scene.surfaces.fill(SushiDSP::room::MaterialId::OpenDoorway);

    expect_every_band_zero(EyringDecay::rt60_seconds(scene));
}

TEST(Small_EyringDecay, EyringDecayHandlesAFullyAbsorbentRoomWithZeroSchroederFrequency)
{
    RoomScene scene = SushiDSP::test::block_room_scene();
    scene.surfaces.fill(SushiDSP::room::MaterialId::OpenDoorway);

    EXPECT_EQ(EyringDecay::schroeder_frequency_hz(scene), 0.0);
}

TEST(Small_EyringDecay, EyringDecayOfAnUnfurnishedSceneIsUnchanged)
{
    /* With nothing furnished the unit evaluates the air-only expression, so the closed form
       matches bit for bit. */
    const RoomScene scene;

    expect_bands_equal_the_closed_form(EyringDecay::rt60_seconds(scene), scene);
}

TEST(Small_EyringDecay, EyringDecayAddsTheFurnishingAreaToTheDenominator)
{
    using SushiDSP::room::FurnitureId;

    RoomScene scene;
    scene.furnishings[0] = {FurnitureId::WardrobeModule, 3.0};
    scene.furnishings[1] = {FurnitureId::SofaTwoSeat, 2.0};
    scene.furnishing_count = 2;
    const double volume = scene.volume_m3();
    const Bands mean_alpha = scene.mean_absorption();
    const Bands furnishing_area = scene.furnishing_absorption_area();

    const Bands computed = EyringDecay::rt60_seconds(scene);

    for (std::size_t band = 0; band < computed.size(); ++band)
    {
        const double expected =
            0.161 * volume /
            (-scene.surface_area_m2() * std::log(1.0 - mean_alpha[band]) + air_term(volume, band) + furnishing_area[band]);
        EXPECT_TRUE(TestSupport::is_within(
            TestSupport::measure_relative_error<double>(computed[band], expected, K_CLOSED_FORM_OPERATIONS)))
            << "band " << band;
    }
}

TEST(Small_EyringDecay, EyringDecayShortensWithAWardrobe)
{
    using SushiDSP::room::FurnitureId;

    RoomScene scene;
    const Bands bare = EyringDecay::rt60_seconds(scene);
    scene.furnishings[0] = {FurnitureId::WardrobeModule, 3.0};
    scene.furnishing_count = 1;

    expect_every_band_below(EyringDecay::rt60_seconds(scene), bare);
}

TEST(Small_EyringDecay, EyringDecayShortensFurtherWithASofaBesideTheWardrobe)
{
    using SushiDSP::room::FurnitureId;

    RoomScene scene;
    scene.furnishings[0] = {FurnitureId::WardrobeModule, 3.0};
    scene.furnishing_count = 1;
    const Bands wardrobed = EyringDecay::rt60_seconds(scene);
    scene.furnishings[1] = {FurnitureId::SofaTwoSeat, 2.0};
    scene.furnishing_count = 2;

    expect_every_band_below(EyringDecay::rt60_seconds(scene), wardrobed);
}

TEST(Small_EyringDecay, EyringDecayShortensFurtherWhenTheFurnishingIsDoubled)
{
    using SushiDSP::room::FurnitureId;

    RoomScene scene;
    scene.furnishings[0] = {FurnitureId::WardrobeModule, 3.0};
    scene.furnishings[1] = {FurnitureId::SofaTwoSeat, 2.0};
    scene.furnishing_count = 2;
    const Bands furnished = EyringDecay::rt60_seconds(scene);
    scene.furnishings[0].quantity = 6.0;
    scene.furnishings[1].quantity = 4.0;

    expect_every_band_below(EyringDecay::rt60_seconds(scene), furnished);
}
