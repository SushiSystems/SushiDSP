/**************************************************************************/
/* test_driver_field.cpp                                                  */
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
 * @file test_driver_field.cpp
 * @brief Tests the cabinet's radiating elements: their shares, their places and the near field.
 * @author Mustafa Garip & Sushi Systems
 */

#include <cmath>
#include <cstddef>
#include <cstdint>

#include <gtest/gtest.h>

#include <SushiDSP/nodes/room/band_response.hpp>
#include <SushiDSP/nodes/room/cabinet_geometry.hpp>
#include <SushiDSP/nodes/room/driver_field.hpp>
#include <SushiDSP/nodes/room/image_source_solver.hpp>
#include <SushiDSP/nodes/room/room_scene.hpp>
#include <SushiDSP/nodes/room/starter_scene.hpp>

using SushiDSP::room::DriverField;
using SushiDSP::room::K_BAND_COUNT;
using SushiDSP::room::Vec3;

namespace
{
    /// @brief Holds the top band's index, 4 kHz.
    constexpr std::size_t K_TOP_BAND = K_BAND_COUNT - 1;

    /// @brief Returns the incoherent level of a field at one band: the root of the summed squares
    /// of share over distance, which is what a band of noise reads.
    double band_level(const DriverField::Field& field, std::size_t band)
    {
        double power = 0.0;
        for (std::uint32_t index = 0; index < field.count; ++index)
        {
            const double pressure = static_cast<double>(field.arrivals[index].gain.gain[band]) /
                                    field.arrivals[index].distance_m;
            power += pressure * pressure;
        }
        return std::sqrt(power);
    }

    /// @brief Returns the in-phase sum of a field at one band: share over distance, added.
    double band_sum(const DriverField::Field& field, std::size_t band)
    {
        double sum = 0.0;
        for (std::uint32_t index = 0; index < field.count; ++index)
        {
            sum += static_cast<double>(field.arrivals[index].gain.gain[band]) /
                   field.arrivals[index].distance_m;
        }
        return sum;
    }
} // namespace

TEST(Unit_Dsp, DriverFieldCountsSevenElementsPerCone)
{
    EXPECT_EQ(DriverField(SushiDSP::room::K_CABINET1X12).element_count(), 7u);
    EXPECT_EQ(DriverField(SushiDSP::room::K_CABINET2X12).element_count(), 14u);
    EXPECT_EQ(DriverField(SushiDSP::room::K_CABINET4X12).element_count(), 28u);
}

TEST(Unit_Dsp, DriverFieldSharesSumToOneInEveryBand)
{
    for (const SushiDSP::room::CabinetGeometry& geometry :
         {SushiDSP::room::K_CABINET1X12, SushiDSP::room::K_CABINET2X12,
          SushiDSP::room::K_CABINET4X12})
    {
        const DriverField field(geometry);
        for (std::size_t band = 0; band < K_BAND_COUNT; ++band)
        {
            double sum = 0.0;
            for (std::uint32_t element = 0; element < field.element_count(); ++element)
            {
                sum += static_cast<double>(field.element_share(element).gain[band]);
            }
            EXPECT_NEAR(sum, 1.0, 1e-5) << "band " << band;
        }
    }
}

TEST(Unit_Dsp, DriverFieldGivesTheCentreMoreOfTheHighsThanOfTheLows)
{
    const DriverField field(SushiDSP::room::K_CABINET1X12);
    const float centre_low = field.element_share(0).gain[0];
    const float centre_high = field.element_share(0).gain[K_TOP_BAND];
    const float ring_low = field.element_share(1).gain[0];
    const float ring_high = field.element_share(1).gain[K_TOP_BAND];

    EXPECT_GT(centre_high, centre_low);
    EXPECT_LT(ring_high, ring_low);
    // Below the breakup the seven elements stand for almost equal areas.
    EXPECT_NEAR(centre_low, ring_low, 0.01f);
}

TEST(Unit_Dsp, DriverFieldPlacesAFourByTwelvesConesInASquareOnTheBaffle)
{
    const SushiDSP::room::CabinetGeometry& geometry = SushiDSP::room::K_CABINET4X12;
    const DriverField field(geometry);
    const Vec3 source{2.0, 2.0, 0.5};
    const double half = 0.5 * geometry.driver_spacing_m;

    // Facing +x, the baffle lies in the y-z plane and the cabinet's left is +y.
    for (std::uint32_t driver = 0; driver < 4; ++driver)
    {
        const Vec3 centre = field.element_position(source, 0.0, driver * 7u);
        EXPECT_NEAR(centre.x, 2.0, 1e-12);
        EXPECT_NEAR(std::fabs(centre.y - 2.0), half, 1e-12);
        EXPECT_NEAR(std::fabs(centre.z - 0.5), half, 1e-12);
    }
    const Vec3 first = field.element_position(source, 0.0, 0u);
    const Vec3 last = field.element_position(source, 0.0, 21u);
    EXPECT_NEAR(std::fabs(first.y - last.y), geometry.driver_spacing_m, 1e-12);
    EXPECT_NEAR(std::fabs(first.z - last.z), geometry.driver_spacing_m, 1e-12);
}

TEST(Unit_Dsp, DriverFieldTurnsWithTheCabinet)
{
    const DriverField field(SushiDSP::room::K_CABINET2X12);
    const Vec3 source{2.0, 2.0, 0.5};
    // Facing +y, the cabinet's left is -x, so the two cones spread along x.
    const Vec3 right = field.element_position(source, 0.5 * 3.14159265358979323846, 0u);
    const Vec3 left = field.element_position(source, 0.5 * 3.14159265358979323846, 7u);

    EXPECT_NEAR(right.y, 2.0, 1e-12);
    EXPECT_NEAR(left.y, 2.0, 1e-12);
    EXPECT_NEAR(right.x - left.x, SushiDSP::room::K_CABINET2X12.driver_spacing_m, 1e-12);
}

TEST(Unit_Dsp, DriverFieldAddsUpToOneOverTheDistanceFarOnAxis)
{
    const DriverField field(SushiDSP::room::K_CABINET4X12);
    const Vec3 source{0.0, 0.0, 0.0};
    const double distance = 50.0;
    const DriverField::Field far = field.toward(source, 0.0, Vec3{distance, 0.0, 0.0});

    for (std::size_t band = 0; band < K_BAND_COUNT; ++band)
    {
        EXPECT_NEAR(band_sum(far, band) * distance, 1.0, 1e-3) << "band " << band;
    }
}

TEST(Unit_Dsp, DriverFieldIsBrighterOnTheDustCapThanAtTheConesEdge)
{
    const SushiDSP::room::CabinetGeometry& geometry = SushiDSP::room::K_CABINET1X12;
    const DriverField field(geometry);
    const Vec3 source{0.0, 0.0, 0.0};
    const DriverField::Field cap = field.toward(source, 0.0, Vec3{0.03, 0.0, 0.0});
    const DriverField::Field edge =
        field.toward(source, 0.0, Vec3{0.03, geometry.cone_radius_m, 0.0});

    // Tilt is the top band over the bottom band; the cap's is the larger.
    const double cap_tilt = band_level(cap, K_TOP_BAND) / band_level(cap, 0);
    const double edge_tilt = band_level(edge, K_TOP_BAND) / band_level(edge, 0);
    EXPECT_GT(cap_tilt, 1.5 * edge_tilt);
}

TEST(Unit_Dsp, DriverFieldStaysFiniteWithTheReceiverOnAnElement)
{
    const DriverField field(SushiDSP::room::K_CABINET1X12);
    const Vec3 source{1.0, 1.0, 1.0};
    const DriverField::Field on = field.toward(source, 0.0, source);

    ASSERT_GT(on.count, 0u);
    EXPECT_DOUBLE_EQ(on.arrivals[0].distance_m, DriverField::K_MIN_DISTANCE_M);
    for (std::uint32_t index = 0; index < on.count; ++index)
    {
        EXPECT_TRUE(std::isfinite(on.arrivals[index].direction.x));
        EXPECT_GE(on.arrivals[index].distance_m, DriverField::K_MIN_DISTANCE_M);
    }
}

TEST(Unit_Dsp, DriverFieldDullsTheHighsBehindTheBaffle)
{
    const DriverField field(SushiDSP::room::K_CABINET1X12);
    const Vec3 source{0.0, 0.0, 0.0};
    const DriverField::Field front = field.toward(source, 0.0, Vec3{1.0, 0.0, 0.0});
    const DriverField::Field behind = field.toward(source, 0.0, Vec3{-1.0, 0.0, 0.0});

    EXPECT_LT(band_sum(behind, K_TOP_BAND), 0.75 * band_sum(front, K_TOP_BAND));
}

TEST(Unit_Dsp, ImageSourceSolverOpensAMicrophonesSetWithOneTapPerElement)
{
    SushiDSP::room::RoomScene scene = SushiDSP::room::starter_scene();
    scene.cabinet = SushiDSP::room::K_CABINET2X12;
    scene.source_position = Vec3{1.5, 2.0, 0.6};
    scene.source_yaw_rad = 0.0;
    scene.microphone_position = Vec3{1.6, 2.0, 0.6};
    scene.microphone_yaw_rad = 3.14159265358979323846;

    SushiDSP::room::ImageSourceSolver::Options options;
    options.listener = SushiDSP::room::ListenerKind::Microphone;
    const SushiDSP::room::EarlyTapSet microphone =
        SushiDSP::room::ImageSourceSolver::solve(scene, options);
    options.listener = SushiDSP::room::ListenerKind::Ears;
    const SushiDSP::room::EarlyTapSet ears =
        SushiDSP::room::ImageSourceSolver::solve(scene, options);

    EXPECT_EQ(microphone.direct_count, 14u);
    EXPECT_EQ(ears.direct_count, 1u);
    ASSERT_GT(microphone.count, microphone.direct_count);
    // Every element is within the baffle's reach of the capsule; a reflection is not.
    for (std::uint32_t index = 0; index < microphone.direct_count; ++index)
    {
        EXPECT_LT(microphone.taps[index].delay_frames, 0.6f * 48000.0f / 343.0f);
        EXPECT_GT(microphone.taps[index].gain, 0.0f);
    }
    for (std::uint32_t index = microphone.direct_count + 1; index < microphone.count; ++index)
    {
        EXPECT_LE(microphone.taps[index - 1].delay_frames, microphone.taps[index].delay_frames);
    }
}
