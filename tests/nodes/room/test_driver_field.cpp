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
 * @file test_driver_field.cpp
 * @brief Tests the cabinet's radiating elements: their shares, their places and the near field.
 * @author Mustafa Garip & Sushi Systems
 */

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>

#include <gtest/gtest.h>

#include <SushiDSP/nodes/room/band_response.hpp>
#include <SushiDSP/nodes/room/cabinet_geometry.hpp>
#include <SushiDSP/nodes/room/driver_field.hpp>
#include <SushiDSP/nodes/room/room_scene.hpp>

#include <support/tolerance.hpp>

using SushiDSP::room::DriverField;
using SushiDSP::room::K_BAND_COUNT;
using SushiDSP::room::Vec3;

namespace
{
    /// @brief Holds the top band's index, 4 kHz.
    constexpr std::size_t K_TOP_BAND = K_BAND_COUNT - 1;

    /// @brief Holds pi.
    constexpr double K_PI = 3.14159265358979323846;

    /// @brief Bounds a coordinate of an element's place: the source sits within 2.5 m of the
    /// origin, one offset is added after at most one rotation by cos and sin of the yaw, and eight
    /// roundings of a number under 2.5 cover the products and sums.
    constexpr double K_POSITION_BOUND = 8.0 * TestSupport::unit_roundoff<double>() * 2.5;

    /// @brief Characterised band: below the breakup the centre and a ring element stand for
    /// almost equal areas, so their shares differ by less than this. Kept from the original test.
    constexpr double K_EQUAL_AREA_BAND = 0.01;

    /// @brief Holds the factor by which the dust cap's tilt must exceed the cone edge's: the
    /// promise the original test stated.
    constexpr double K_CAP_TILT_FACTOR = 1.5;

    /// @brief Holds the share of the front's top band that is the most the back may carry.
    constexpr double K_BEHIND_FACTOR = 0.75;

    /// @brief Holds one shipped geometry.
    struct GeometryRow
    {
            const char* name;
            SushiDSP::room::CabinetGeometry geometry;
    };

    /// @brief Holds one cone of the 4x12, by the index of its first element.
    struct ConeRow
    {
            const char* name;
            std::uint32_t driver;
    };

    /// @brief Names a row for gtest.
    template <typename Row>
    std::string row_name(const ::testing::TestParamInfo<Row>& info)
    {
        return info.param.name;
    }

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

    /// @brief Expects @p computed to be @p reference within K_POSITION_BOUND.
    void expect_place(double computed, double reference)
    {
        EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(computed, reference, K_POSITION_BOUND, 1.0)));
    }

    /// @brief Expects the shares of a field built on @p geometry to sum to one in every band,
    /// to the float rounding of their count.
    void expect_shares_sum_to_one(const SushiDSP::room::CabinetGeometry& geometry)
    {
        const DriverField field(geometry);
        for (std::size_t band = 0; band < K_BAND_COUNT; ++band)
        {
            double sum = 0.0;
            for (std::uint32_t element = 0; element < field.element_count(); ++element)
            {
                sum += static_cast<double>(field.element_share(element).gain[band]);
            }
            EXPECT_TRUE(TestSupport::is_within(
                TestSupport::measure_sum_error<float>(sum, 1.0, 1.0, field.element_count())))
                << "band " << band;
        }
    }

    /// @brief Expects the in-phase sum of a far field to be one over its distance in every band.
    void expect_far_field_is_one_over_distance(const DriverField::Field& far, double distance, double bound)
    {
        for (std::size_t band = 0; band < K_BAND_COUNT; ++band)
        {
            EXPECT_TRUE(TestSupport::is_within(
                TestSupport::measure_error(band_sum(far, band) * distance, 1.0, bound, 1.0)))
                << "band " << band;
        }
    }

    /// @brief Expects every arrival of @p field to have a finite direction and a distance of at
    /// least K_MIN_DISTANCE_M.
    void expect_arrivals_finite_and_clamped(const DriverField::Field& field)
    {
        for (std::uint32_t index = 0; index < field.count; ++index)
        {
            EXPECT_TRUE(std::isfinite(field.arrivals[index].direction.x));
            EXPECT_TRUE(std::isfinite(field.arrivals[index].direction.y));
            EXPECT_TRUE(std::isfinite(field.arrivals[index].direction.z));
            EXPECT_GE(field.arrivals[index].distance_m, DriverField::K_MIN_DISTANCE_M);
        }
    }

    class Small_DriverFieldShares : public ::testing::TestWithParam<GeometryRow>
    {
    };

    class Small_DriverFieldCone : public ::testing::TestWithParam<ConeRow>
    {
    };
} // namespace

TEST(Small_DriverField, DriverFieldCountsSevenElementsPerCone)
{
    EXPECT_EQ(DriverField(SushiDSP::room::K_CABINET1X12).element_count(), 7u);
    EXPECT_EQ(DriverField(SushiDSP::room::K_CABINET2X12).element_count(), 14u);
    EXPECT_EQ(DriverField(SushiDSP::room::K_CABINET4X12).element_count(), 28u);
}

TEST_P(Small_DriverFieldShares, DriverFieldSharesSumToOneInEveryBand)
{
    expect_shares_sum_to_one(GetParam().geometry);
}

INSTANTIATE_TEST_SUITE_P(Cabinets, Small_DriverFieldShares,
                         ::testing::Values(GeometryRow{"OneByTwelve", SushiDSP::room::K_CABINET1X12},
                                           GeometryRow{"TwoByTwelve", SushiDSP::room::K_CABINET2X12},
                                           GeometryRow{"FourByTwelve", SushiDSP::room::K_CABINET4X12}),
                         row_name<GeometryRow>);

TEST(Small_DriverField, DriverFieldGivesTheCentreMoreOfTheHighsThanOfTheLows)
{
    const DriverField field(SushiDSP::room::K_CABINET1X12);

    EXPECT_GT(field.element_share(0).gain[K_TOP_BAND], field.element_share(0).gain[0]);
}

TEST(Small_DriverField, DriverFieldGivesARingElementMoreOfTheLowsThanOfTheHighs)
{
    const DriverField field(SushiDSP::room::K_CABINET1X12);

    EXPECT_LT(field.element_share(1).gain[K_TOP_BAND], field.element_share(1).gain[0]);
}

TEST(Small_DriverField, DriverFieldGivesTheCentreAndARingElementAlmostEqualLowsBelowTheBreakup)
{
    // Below the breakup the seven elements stand for almost equal areas.
    const DriverField field(SushiDSP::room::K_CABINET1X12);

    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(
        field.element_share(0).gain[0], field.element_share(1).gain[0], K_EQUAL_AREA_BAND, 1.0)));
}

TEST_P(Small_DriverFieldCone, DriverFieldPlacesAFourByTwelvesConeAHalfSpacingFromTheBaffleCentre)
{
    const SushiDSP::room::CabinetGeometry& geometry = SushiDSP::room::K_CABINET4X12;
    const DriverField field(geometry);
    const Vec3 source{2.0, 2.0, 0.5};
    const double half = 0.5 * geometry.driver_spacing_m;

    // Facing +x, the baffle lies in the y-z plane and the cabinet's left is +y.
    const Vec3 centre = field.element_position(source, 0.0, GetParam().driver * 7u);

    expect_place(centre.x, 2.0);
    expect_place(std::fabs(centre.y - 2.0), half);
    expect_place(std::fabs(centre.z - 0.5), half);
}

INSTANTIATE_TEST_SUITE_P(Cones, Small_DriverFieldCone,
                         ::testing::Values(ConeRow{"Cone0", 0u}, ConeRow{"Cone1", 1u},
                                           ConeRow{"Cone2", 2u}, ConeRow{"Cone3", 3u}),
                         row_name<ConeRow>);

TEST(Small_DriverField, DriverFieldSpacesTheFirstAndLastConeByTheDriverSpacingInBothAxes)
{
    const SushiDSP::room::CabinetGeometry& geometry = SushiDSP::room::K_CABINET4X12;
    const DriverField field(geometry);
    const Vec3 source{2.0, 2.0, 0.5};

    const Vec3 first = field.element_position(source, 0.0, 0u);
    const Vec3 last = field.element_position(source, 0.0, 21u);

    expect_place(std::fabs(first.y - last.y), geometry.driver_spacing_m);
    expect_place(std::fabs(first.z - last.z), geometry.driver_spacing_m);
}

TEST(Small_DriverField, DriverFieldTurnsWithTheCabinet)
{
    const DriverField field(SushiDSP::room::K_CABINET2X12);
    const Vec3 source{2.0, 2.0, 0.5};
    // Facing +y, the cabinet's left is -x, so the two cones spread along x.
    const Vec3 right = field.element_position(source, 0.5 * K_PI, 0u);
    const Vec3 left = field.element_position(source, 0.5 * K_PI, 7u);

    expect_place(right.y, 2.0);
    expect_place(left.y, 2.0);
    expect_place(right.x - left.x, SushiDSP::room::K_CABINET2X12.driver_spacing_m);
}

TEST(Small_DriverField, DriverFieldAddsUpToOneOverTheDistanceFarOnAxis)
{
    const SushiDSP::room::CabinetGeometry& geometry = SushiDSP::room::K_CABINET4X12;
    const DriverField field(geometry);
    const double distance = 50.0;
    const DriverField::Field far = field.toward(Vec3{0.0, 0.0, 0.0}, 0.0, Vec3{distance, 0.0, 0.0});

    /* An element at offsets y and z from the axis is r = d sqrt(1 + (y^2 + z^2) / d^2) away, so
       1 / r differs from 1 / d by at most (y^2 + z^2) / (2 d^2) of itself; the farthest element
       stands half a spacing and a ring radius off the axis in both. The shares add float roundings. */
    const double offset = 0.5 * geometry.driver_spacing_m + DriverField::K_RING_RADIUS_RATIO * geometry.cone_radius_m;
    const double bound = offset * offset / (distance * distance) +
                         static_cast<double>(field.element_count()) * TestSupport::unit_roundoff<float>();
    expect_far_field_is_one_over_distance(far, distance, bound);
}

TEST(Small_DriverField, DriverFieldIsBrighterOnTheDustCapThanAtTheConesEdge)
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
    EXPECT_GT(cap_tilt, K_CAP_TILT_FACTOR * edge_tilt);
}

TEST(Small_DriverField, DriverFieldClampsTheNearestArrivalToTheMinimumDistanceWithTheReceiverOnAnElement)
{
    const DriverField field(SushiDSP::room::K_CABINET1X12);
    const Vec3 source{1.0, 1.0, 1.0};

    const DriverField::Field on = field.toward(source, 0.0, source);

    ASSERT_GT(on.count, 0u);
    EXPECT_EQ(on.arrivals[0].distance_m, DriverField::K_MIN_DISTANCE_M);
}

TEST(Small_DriverField, DriverFieldStaysFiniteWithTheReceiverOnAnElement)
{
    const DriverField field(SushiDSP::room::K_CABINET1X12);
    const Vec3 source{1.0, 1.0, 1.0};

    expect_arrivals_finite_and_clamped(field.toward(source, 0.0, source));
}

TEST(Small_DriverField, DriverFieldDullsTheHighsBehindTheBaffle)
{
    const DriverField field(SushiDSP::room::K_CABINET1X12);
    const Vec3 source{0.0, 0.0, 0.0};
    const DriverField::Field front = field.toward(source, 0.0, Vec3{1.0, 0.0, 0.0});
    const DriverField::Field behind = field.toward(source, 0.0, Vec3{-1.0, 0.0, 0.0});

    EXPECT_LT(band_sum(behind, K_TOP_BAND), K_BEHIND_FACTOR * band_sum(front, K_TOP_BAND));
}
