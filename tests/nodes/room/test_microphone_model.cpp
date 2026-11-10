/**************************************************************************/
/* test_microphone_model.cpp                                              */
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
 * @file test_microphone_model.cpp
 * @brief Tests for MicrophoneModel, the first-order pattern with its proximity effect.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

#include <SushiDSP/nodes/room/band_response.hpp>
#include <SushiDSP/nodes/room/microphone_model.hpp>

#include "support/tolerance.hpp"

namespace
{
    using SushiDSP::room::BandResponse;
    using SushiDSP::room::MicrophoneKind;
    using SushiDSP::room::MicrophoneModel;
    using SushiDSP::room::PolarPattern;

    /// @brief Holds pi.
    constexpr double K_PI = 3.14159265358979323846;

    /// @brief Holds the decibels one neper of ratio is worth: 20 / ln 10.
    constexpr double K_DECIBELS_PER_NEPER = 8.685889638065035;

    /// @brief Holds the distance of the far-field reading, in metres.
    constexpr double K_FAR_METRES = 3.0;

    /// @brief Counts the bands.
    constexpr std::size_t K_BANDS = SushiDSP::room::K_BAND_COUNT;

    /// @brief Holds a flat on-axis curve.
    constexpr std::array<float, K_BANDS> K_FLAT_CURVE = {0, 0, 0, 0, 0, 0};

    /// @brief Returns half a unit of the last digit of a figure typed with the given number of
    /// decimals, the most a figure rounded to them can sit from the exact value.
    double half_unit_of_last_digit(int decimals)
    {
        return 0.5 * std::pow(10.0, -decimals);
    }

    /// @brief Returns the decibels a float reading of a gain can sit off: each of two gains is cast
    /// to float, 1 u of ratio, worth K_DECIBELS_PER_NEPER u, and the result is cast to float,
    /// u of its size.
    double float_slack_db(double decibels)
    {
        return (2.0 * K_DECIBELS_PER_NEPER + std::fabs(decibels)) *
               TestSupport::unit_roundoff<float>();
    }

    /// @brief Returns a cardioid's on-axis gain in decibels at one frequency and distance,
    /// 0.5 + 0.5 * B(f, r), the far-field value being 1.
    double cardioid_on_axis_db(double frequency_hz, double distance_m)
    {
        return 20.0 * std::log10(0.5 + 0.5 * MicrophoneModel::proximity_boost(frequency_hz,
                                                                                 distance_m));
    }

    /// @brief Returns the cardioid model with a flat curve.
    MicrophoneModel flat_cardioid()
    {
        return MicrophoneModel(PolarPattern::Cardioid, K_FLAT_CURVE);
    }

    /// @brief Returns the figure-eight model at its side, ninety degrees off axis.
    BandResponse figure_eight_side()
    {
        const MicrophoneModel eight(PolarPattern::FigureEight, K_FLAT_CURVE);
        return eight.gain_for(K_PI / 2.0, 1.0);
    }

    /// @brief Holds one proximity boost reading: a frequency, a distance, the figure worked out by
    /// hand and the decimals it was written with.
    struct BoostReading
    {
            const char* name;
            double frequency_hz;
            double distance_m;
            double expected;
            int decimals;
    };

    /// @brief Holds the boost readings worked out by hand: sqrt(1 + (343 / (2 pi f r))^2).
    /// 343 / (2 pi * 100 * 0.05) = 10.9180 and sqrt(1 + 119.204) = 10.9637; 343 / (2 pi * 100 *
    /// 0.30) = 1.81967 and sqrt(1 + 3.31120) = 2.07634; 343 / (2 pi * 1000 * 3) = 0.018197 and
    /// sqrt(1 + 3.3112e-4) = 1.000166.
    const std::array<BoostReading, 3> kBoostReadings = {
        {{"HundredHertzAtFiveCentimetres", 100.0, 0.05, 10.9637, 4},
         {"HundredHertzAtThirtyCentimetres", 100.0, 0.30, 2.07634, 5},
         {"ThousandHertzAtThreeMetres", 1000.0, 3.0, 1.000166, 6}}};

    /// @brief Names a boost reading after its case.
    std::string boost_name(const ::testing::TestParamInfo<BoostReading>& info)
    {
        return info.param.name;
    }

    /// @brief Holds one cardioid distance and the gain it adds over the far field at 100 Hz, in
    /// decibels, worked out by hand to two decimals.
    struct ProximityGain
    {
            const char* name;
            double distance_m;
            double over_far_db;
    };

    /// @brief Holds the 100 Hz gains: 0.5 + 0.5 B is 5.9819 (15.54 dB) at 5 cm, 1.53817 (3.74 dB)
    /// at 30 cm and 1.0082 (0.07 dB) at 3 m, so the capsule gains 15.47 dB and 3.67 dB over 3 m.
    const std::array<ProximityGain, 2> kProximityGains = {
        {{"FiveCentimetres", 0.05, 15.47}, {"ThirtyCentimetres", 0.30, 3.67}}};

    /// @brief Names a proximity gain after its distance.
    std::string proximity_name(const ::testing::TestParamInfo<ProximityGain>& info)
    {
        return info.param.name;
    }

    /// @brief Holds one distance and the first band's gain at it, in decibels, worked out by hand to
    /// two decimals, in absolute terms and over the same capsule at three metres.
    struct BandGain
    {
            const char* name;
            double distance_m;
            double absolute_db;
            double over_far_db;
    };

    /// @brief Holds the 125 Hz gains: the boost is 8.7915 at 5 cm, 1.76612 at 30 cm and 1.01054 at
    /// 3 m, so the model reads 4.8957 (13.80 dB), 1.38306 (2.82 dB) and 1.00527 (0.046 dB).
    const std::array<BandGain, 2> kBandGains = {
        {{"FiveCentimetres", 0.05, 13.80, 13.75}, {"ThirtyCentimetres", 0.30, 2.82, 2.77}}};

    /// @brief Names a band gain after its distance.
    std::string band_gain_name(const ::testing::TestParamInfo<BandGain>& info)
    {
        return info.param.name;
    }

    /// @brief Holds one incidence and one distance the omni is read at.
    struct OmniReading
    {
            const char* name;
            double incidence_rad;
            double distance_m;
    };

    /// @brief Holds the incidences and distances the omni is read at.
    const std::array<OmniReading, 6> kOmniReadings = {{{"OnAxisAtFiveCentimetres", 0.0, 0.05},
                                                       {"OnAxisAtThreeMetres", 0.0, 3.0},
                                                       {"SixtyDegreesAtFiveCentimetres", K_PI / 3.0, 0.05},
                                                       {"SixtyDegreesAtThreeMetres", K_PI / 3.0, 3.0},
                                                       {"BehindAtFiveCentimetres", K_PI, 0.05},
                                                       {"BehindAtThreeMetres", K_PI, 3.0}}};

    /// @brief Names an omni reading after its case.
    std::string omni_name(const ::testing::TestParamInfo<OmniReading>& info)
    {
        return info.param.name;
    }

    /// @brief Holds one distance the on-axis curve is read at.
    struct CurveDistance
    {
            const char* name;
            double distance_m;
    };

    /// @brief Holds the distances the on-axis curve is read at.
    const std::array<CurveDistance, 3> kCurveDistances = {
        {{"FiveCentimetres", 0.05}, {"ThirtyCentimetres", 0.30}, {"ThreeMetres", 3.0}}};

    /// @brief Names a curve distance after its case.
    std::string curve_name(const ::testing::TestParamInfo<CurveDistance>& info)
    {
        return info.param.name;
    }

    /// @brief Holds one pattern and the pressure share a of P(theta) = a + (1 - a) cos(theta) it
    /// is documented with.
    struct PatternShare
    {
            const char* name;
            PolarPattern pattern;
            double share;
    };

    /// @brief Holds the shares the header documents: 1, 0.5, 0.366 and 0.
    const std::array<PatternShare, 4> kPatternShares = {{{"Omni", PolarPattern::Omni, 1.0},
                                                         {"Cardioid", PolarPattern::Cardioid, 0.5},
                                                         {"Supercardioid", PolarPattern::Supercardioid, 0.366},
                                                         {"FigureEight", PolarPattern::FigureEight, 0.0}}};

    /// @brief Names a pattern share after its pattern.
    std::string share_name(const ::testing::TestParamInfo<PatternShare>& info)
    {
        return info.param.name;
    }

    /// @brief Returns every shipped microphone kind.
    std::vector<MicrophoneKind> all_kinds()
    {
        std::vector<MicrophoneKind> kinds;
        for (std::size_t index = 0; index < SushiDSP::room::K_MICROPHONE_KIND_COUNT; ++index)
        {
            kinds.push_back(static_cast<MicrophoneKind>(index));
        }
        return kinds;
    }

    /// @brief Names a kind case after its position in the table.
    std::string kind_name(const ::testing::TestParamInfo<MicrophoneKind>& info)
    {
        return "Kind" + std::to_string(info.index);
    }

    /// @brief Returns how many times its bound the worst band's colour error reads: the
    /// difference of the coloured and the flat cardioid in decibels against the curve.
    double worst_colour_error_ratio(double distance_m)
    {
        const std::array<float, K_BANDS> curve = {0, 0, 0, 0, 3, 6};
        const std::array<float, K_BANDS> flat_db = flat_cardioid().gain_for(0.3, distance_m).decibels();
        const std::array<float, K_BANDS> coloured_db =
            MicrophoneModel(PolarPattern::Cardioid, curve).gain_for(0.3, distance_m).decibels();
        double worst = 0.0;
        for (std::size_t band = 0; band < K_BANDS; ++band)
        {
            const double bound = float_slack_db(std::fabs(coloured_db[band]) + std::fabs(flat_db[band]));
            worst = std::max(worst, TestSupport::measure_error(coloured_db[band] - flat_db[band],
                                                               static_cast<double>(curve[band]), bound)
                                        .ratio);
        }
        return worst;
    }
} // namespace

class Small_MicrophoneModelBoost : public ::testing::TestWithParam<BoostReading>
{
};

class Small_MicrophoneModelProximity : public ::testing::TestWithParam<ProximityGain>
{
};

class Small_MicrophoneModelBand : public ::testing::TestWithParam<BandGain>
{
};

class Small_MicrophoneModelOmni : public ::testing::TestWithParam<OmniReading>
{
};

class Small_MicrophoneModelCurve : public ::testing::TestWithParam<CurveDistance>
{
};

class Small_MicrophoneModelShare : public ::testing::TestWithParam<PatternShare>
{
};

class Small_MicrophoneTable : public ::testing::TestWithParam<MicrophoneKind>
{
};

TEST_P(Small_MicrophoneModelBoost, ProximityBoostIsTheSphericalWaveTerm)
{
    // sqrt(1 + 10.9180^2) = 10.9637 at 100 Hz and 5 cm. See tests/nodes/room/README.md.
    EXPECT_NEAR(MicrophoneModel::proximity_boost(GetParam().frequency_hz, GetParam().distance_m),
                GetParam().expected, half_unit_of_last_digit(GetParam().decimals));
}

INSTANTIATE_TEST_SUITE_P(Readings, Small_MicrophoneModelBoost, ::testing::ValuesIn(kBoostReadings),
                         boost_name);

TEST_P(Small_MicrophoneModelProximity, CardioidGainsTheWorkedOutDecibelsOverTheFarFieldAtOneHundredHertz)
{
    // At 100 Hz the two distances differ by 15.47 dB. See tests/nodes/room/README.md.
    const double over_far =
        cardioid_on_axis_db(100.0, GetParam().distance_m) - cardioid_on_axis_db(100.0, K_FAR_METRES);
    EXPECT_NEAR(over_far, GetParam().over_far_db, half_unit_of_last_digit(2));
}

INSTANTIATE_TEST_SUITE_P(Distances, Small_MicrophoneModelProximity,
                         ::testing::ValuesIn(kProximityGains), proximity_name);

TEST(Small_MicrophoneModel, CardioidAtThirtyCentimetresGainsThreePointSevenFourDecibelsAtOneHundredHertz)
{
    // At 100 Hz and 30 cm the capsule gains 3.74 dB. See tests/nodes/room/README.md.
    EXPECT_NEAR(cardioid_on_axis_db(100.0, 0.30), 3.74, half_unit_of_last_digit(2));
}

TEST_P(Small_MicrophoneModelBand, ModelGainAtTheFirstBandIsTheWorkedOutDecibels)
{
    // At 125 Hz and 5 cm the model reads 13.80 dB. See tests/nodes/room/README.md.
    const double got = flat_cardioid().gain_for(0.0, GetParam().distance_m).decibels()[0];
    EXPECT_NEAR(got, GetParam().absolute_db,
                half_unit_of_last_digit(2) + float_slack_db(GetParam().absolute_db));
}

TEST_P(Small_MicrophoneModelBand, ModelGainOverTheFarFieldIsTheWorkedOutDecibels)
{
    const MicrophoneModel cardioid = flat_cardioid();
    const double near_db = cardioid.gain_for(0.0, GetParam().distance_m).decibels()[0];
    const double far_db = cardioid.gain_for(0.0, K_FAR_METRES).decibels()[0];
    EXPECT_NEAR(near_db - far_db, GetParam().over_far_db,
                half_unit_of_last_digit(2) + float_slack_db(GetParam().over_far_db));
}

INSTANTIATE_TEST_SUITE_P(Distances, Small_MicrophoneModelBand, ::testing::ValuesIn(kBandGains),
                         band_gain_name);

TEST(Small_MicrophoneModel, FigureEightSideReadsAsTheDecibelFloorBand)
{
    // The pattern term floors at K_DECIBEL_FLOOR. See tests/nodes/room/README.md.
    const std::array<float, K_BANDS> floor = {
        SushiDSP::room::K_DECIBEL_FLOOR, SushiDSP::room::K_DECIBEL_FLOOR,
        SushiDSP::room::K_DECIBEL_FLOOR, SushiDSP::room::K_DECIBEL_FLOOR,
        SushiDSP::room::K_DECIBEL_FLOOR, SushiDSP::room::K_DECIBEL_FLOOR};
    EXPECT_EQ(figure_eight_side().decibels(), floor);
}

TEST(Small_MicrophoneModel, FigureEightSideIsNeverANaN)
{
    const BandResponse side = figure_eight_side();
    EXPECT_TRUE(std::none_of(side.gain.begin(), side.gain.end(),
                             [](float gain) { return std::isnan(gain); }));
}

TEST(Small_MicrophoneModel, FigureEightSideGainIsTheResidueOfCosineOfAQuarterTurn)
{
    const double bound = std::fabs(std::cos(K_PI / 2.0)) *
                         MicrophoneModel::proximity_boost(SushiDSP::room::K_BAND_CENTRES_HZ[0], 1.0);
    const BandResponse side = figure_eight_side();
    EXPECT_LE(static_cast<double>(*std::max_element(side.gain.begin(), side.gain.end())),
              bound * (1.0 + 2.0 * TestSupport::unit_roundoff<float>()));
}

TEST(Small_MicrophoneModel, FigureEightRejectsNinetyDegreesByAtLeastFortyDecibels)
{
    const MicrophoneModel eight(PolarPattern::FigureEight, K_FLAT_CURVE);
    const std::array<float, K_BANDS> front_db = eight.gain_for(0.0, 1.0).decibels();
    const std::array<float, K_BANDS> side_db = figure_eight_side().decibels();
    std::array<float, K_BANDS> rejection{};
    std::transform(front_db.begin(), front_db.end(), side_db.begin(), rejection.begin(),
                   [](float front, float side) { return front - side; });
    EXPECT_GE(*std::min_element(rejection.begin(), rejection.end()), 40.0f);
}

TEST_P(Small_MicrophoneModelOmni, OmniGainIsUnityAtEveryBand)
{
    const MicrophoneModel omni(PolarPattern::Omni, K_FLAT_CURVE);
    const BandResponse gain = omni.gain_for(GetParam().incidence_rad, GetParam().distance_m);
    EXPECT_TRUE(std::all_of(gain.gain.begin(), gain.gain.end(),
                            [](float value) { return value == 1.0f; }));
}

INSTANTIATE_TEST_SUITE_P(Readings, Small_MicrophoneModelOmni, ::testing::ValuesIn(kOmniReadings),
                         omni_name);

TEST_P(Small_MicrophoneModelCurve, AppliesItsOnAxisCurveAtEveryDistanceOffAxis)
{
    EXPECT_LE(worst_colour_error_ratio(GetParam().distance_m), TestSupport::K_DEFAULT_THRESHOLD);
}

INSTANTIATE_TEST_SUITE_P(Distances, Small_MicrophoneModelCurve, ::testing::ValuesIn(kCurveDistances),
                         curve_name);

TEST_P(Small_MicrophoneModelShare, PressureShareIsTheDocumentedOne)
{
    EXPECT_EQ(SushiDSP::room::pattern_pressure_share(GetParam().pattern), GetParam().share);
}

INSTANTIATE_TEST_SUITE_P(Patterns, Small_MicrophoneModelShare, ::testing::ValuesIn(kPatternShares),
                         share_name);

TEST_P(Small_MicrophoneTable, FindsEveryRowByItsOwnKey)
{
    const SushiDSP::room::MicrophonePreset& preset = SushiDSP::room::microphone_preset(GetParam());
    MicrophoneKind found{};
    ASSERT_TRUE(SushiDSP::room::find_microphone(preset.key, found)) << preset.key;
    EXPECT_EQ(found, GetParam());
}

TEST_P(Small_MicrophoneTable, NamesItsTransducerAndItsCitation)
{
    const SushiDSP::room::MicrophonePreset& preset = SushiDSP::room::microphone_preset(GetParam());
    EXPECT_STRNE(preset.name, "");
    EXPECT_STRNE(preset.transducer, "");
    EXPECT_STRNE(preset.citation, "");
}

INSTANTIATE_TEST_SUITE_P(Kinds, Small_MicrophoneTable, ::testing::ValuesIn(all_kinds()), kind_name);

TEST(Small_MicrophoneModel, UnknownKeyIsNotFoundAndLeavesTheKindAlone)
{
    MicrophoneKind untouched = MicrophoneKind::FlatCondenser;
    EXPECT_FALSE(SushiDSP::room::find_microphone("sm57", untouched));
    EXPECT_EQ(untouched, MicrophoneKind::FlatCondenser);
}

TEST(Small_MicrophoneModel, FlatCondenserTransducesAsACondenser)
{
    EXPECT_STREQ(SushiDSP::room::microphone_preset(MicrophoneKind::FlatCondenser).transducer,
                 "condenser");
}
