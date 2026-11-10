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
 * @file test_medium_absorption.cpp
 * @brief Tests for MediumAbsorption, the air and furnishing loss of one room scene.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

#include <SushiDSP/nodes/room/air_absorption.hpp>
#include <SushiDSP/nodes/room/furniture.hpp>
#include <SushiDSP/nodes/room/medium_absorption.hpp>
#include <SushiDSP/nodes/room/room_scene.hpp>

#include "support/tolerance.hpp"

namespace
{
    using SushiDSP::room::AirAbsorption;
    using SushiDSP::room::BandResponse;
    using SushiDSP::room::FurnitureId;
    using SushiDSP::room::K_BAND_COUNT;
    using SushiDSP::room::MediumAbsorption;
    using SushiDSP::room::RoomScene;

    /// @brief Counts the double roundings between a furnishing coefficient and its hand-worked
    /// value: two products, a sum and a quotient.
    constexpr std::size_t K_FURNISHING_OPERATIONS = 4;

    /// @brief Counts the double roundings of the energy coefficient: those of the furnishing
    /// coefficient and the sum with the air's.
    constexpr std::size_t K_ENERGY_OPERATIONS = K_FURNISHING_OPERATIONS + 1;

    /// @brief Counts the float roundings between the path-gain ratio and the exponential: the cast of
    /// the furnishing factor, the product with the air's gain, and the quotient the test takes.
    constexpr std::size_t K_RATIO_OPERATIONS = 4;

    /// @brief Holds the two path lengths the path-gain tests read, in metres.
    constexpr double K_SHORT_PATH_M = 2.6;
    constexpr double K_LONG_PATH_M = 12.0;

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
        const double wardrobe =
            static_cast<double>(SushiDSP::room::furniture(FurnitureId::WardrobeModule).absorption_area[band]);
        const double sofa = static_cast<double>(SushiDSP::room::furniture(FurnitureId::SofaTwoSeat).absorption_area[band]);
        return (3.0 * wardrobe + 2.0 * sofa) / (4.0 * 60.0);
    }

    /// @brief Expects a scene's medium to match the air's, band by band, for coefficients and
    /// bit for bit for path gains.
    void expect_matches_air(const MediumAbsorption& medium)
    {
        for (std::size_t band = 0; band < K_BAND_COUNT; ++band)
        {
            EXPECT_EQ(medium.energy_coefficient_per_metre(band), AirAbsorption::energy_coefficient_per_metre(band))
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

    /// @brief Counts the bands and distances, on a half-metre grid to thirty metres, at which the
    /// path gain is higher than at the distance before it.
    std::size_t gain_rises_with_distance(const MediumAbsorption& medium)
    {
        std::size_t rises = 0;
        std::array<float, K_BAND_COUNT> previous = medium.over_path(0.0).gain;
        for (double metres = 0.5; metres <= 30.0; metres += 0.5)
        {
            const BandResponse current = medium.over_path(metres);
            for (std::size_t band = 0; band < K_BAND_COUNT; ++band)
            {
                if (current.gain[band] > previous[band])
                {
                    ++rises;
                }
            }
            previous = current.gain;
        }
        return rises;
    }

    /// @brief Returns the ratio of the furnished path gain to the air's in one band, and the
    /// exponential the furnishings should contribute, as a pair.
    std::array<double, 2> ratio_and_expected(std::size_t band, double metres)
    {
        const MediumAbsorption medium(furnished_scene());
        const BandResponse got = medium.over_path(metres);
        const BandResponse air = AirAbsorption::over_path(metres);
        return {static_cast<double>(got.gain[band]) / static_cast<double>(air.gain[band]),
                std::exp(-0.5 * expected_furnishing(band) * metres)};
    }

    /// @brief Names a per-band case after its band index.
    std::string band_name(const ::testing::TestParamInfo<std::size_t>& info)
    {
        return "Band" + std::to_string(info.param);
    }

    const std::vector<std::size_t> kBands = {0, 1, 2, 3, 4, 5};
} // namespace

TEST(Small_MediumAbsorption, MediumAbsorptionOfAnUnfurnishedSceneIsTheAirsOwn)
{
    expect_matches_air(MediumAbsorption(RoomScene{}));
}

TEST(Small_MediumAbsorption, MediumAbsorptionTreatsACountOfZeroAsUnfurnished)
{
    RoomScene scene = furnished_scene();
    scene.furnishing_count = 0;
    expect_matches_air(MediumAbsorption(scene));
}

class Small_MediumAbsorptionBands : public ::testing::TestWithParam<std::size_t>
{
};

TEST_P(Small_MediumAbsorptionBands, MediumAbsorptionFurnishingCoefficientIsAreaOverFourVolumes)
{
    const MediumAbsorption medium(furnished_scene());

    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_relative_error<double>(
        medium.furnishing_coefficient_per_metre(GetParam()), expected_furnishing(GetParam()), K_FURNISHING_OPERATIONS)));
}

TEST_P(Small_MediumAbsorptionBands, MediumAbsorptionFurnishingCoefficientIsPositive)
{
    EXPECT_GT(MediumAbsorption(furnished_scene()).furnishing_coefficient_per_metre(GetParam()), 0.0);
}

TEST_P(Small_MediumAbsorptionBands, MediumAbsorptionEnergyCoefficientIsTheAirPlusTheFurnishings)
{
    const MediumAbsorption medium(furnished_scene());
    const double expected =
        AirAbsorption::energy_coefficient_per_metre(GetParam()) + expected_furnishing(GetParam());

    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_relative_error<double>(
        medium.energy_coefficient_per_metre(GetParam()), expected, K_ENERGY_OPERATIONS)));
}

TEST_P(Small_MediumAbsorptionBands, MediumAbsorptionPathGainOverAShortPathIsTheAirsTimesHalfTheFurnishingDecay)
{
    const std::array<double, 2> pair = ratio_and_expected(GetParam(), K_SHORT_PATH_M);

    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_relative_error<float>(pair[0], pair[1], K_RATIO_OPERATIONS)));
}

TEST_P(Small_MediumAbsorptionBands, MediumAbsorptionPathGainOverALongPathIsTheAirsTimesHalfTheFurnishingDecay)
{
    const std::array<double, 2> pair = ratio_and_expected(GetParam(), K_LONG_PATH_M);

    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_relative_error<float>(pair[0], pair[1], K_RATIO_OPERATIONS)));
}

INSTANTIATE_TEST_SUITE_P(Bands, Small_MediumAbsorptionBands, ::testing::ValuesIn(kBands), band_name);

TEST(Small_MediumAbsorption, MediumAbsorptionOverZeroMetresIsUnityInEveryBand)
{
    const BandResponse none = MediumAbsorption(furnished_scene()).over_path(0.0);

    EXPECT_EQ(none.gain, (std::array<float, K_BAND_COUNT>{1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f}));
}

TEST(Small_MediumAbsorption, MediumAbsorptionGainNeverRisesWithDistance)
{
    EXPECT_EQ(gain_rises_with_distance(MediumAbsorption(furnished_scene())), 0u);
}
