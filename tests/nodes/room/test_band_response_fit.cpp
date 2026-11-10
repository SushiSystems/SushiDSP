/**************************************************************************/
/* test_band_response_fit.cpp                                             */
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
 * @file test_band_response_fit.cpp
 * @brief Tests for BandResponseFit, the two-biquad fit of six band gains.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

#include <SushiDSP/nodes/room/air_absorption.hpp>
#include <SushiDSP/nodes/room/band_response_fit.hpp>
#include <SushiDSP/nodes/room/materials.hpp>

#include <support/tolerance.hpp>

namespace
{
    using SushiDSP::room::AirAbsorption;
    using SushiDSP::room::BandResponse;
    using SushiDSP::room::BandResponseFit;
    using SushiDSP::room::K_BAND_CENTRES_HZ;
    using SushiDSP::room::MaterialId;
    using Decibels = std::array<float, SushiDSP::room::K_BAND_COUNT>;

    /// @brief Visits one target at one sample rate, named by a label.
    using TargetVisitor = std::function<void(const BandResponse&, double, const std::string&)>;

    /// @brief Characterised band: a fit lands within this many decibels of a material target
    /// at every centre. Kept from the original test; the spec asks for one decibel.
    constexpr double K_FIT_BAND_DB = 1.0;

    /// @brief Characterised band: a flat target is reproduced to this many decibels. Kept from
    /// the original test.
    constexpr double K_FLAT_BAND_DB = 0.01;

    /// @brief Characterised band: the fit of a dip between flat bands lands this close, in
    /// decibels. Kept from the original test.
    constexpr double K_DIP_BAND_DB = 0.5;

    /// @brief Characterised bound on how far a coefficient moves for a 0.05 dB nudge of one
    /// band. 0.05 dB is the node's carry threshold; the worst move measured by the original
    /// test was 0.128, at 96 kHz.
    constexpr double K_SMALL_NUDGE_MOVE = 0.2;

    /// @brief Characterised bound on how far a coefficient moves for a 0.5 dB nudge. Kept from
    /// the original test.
    constexpr double K_LARGE_NUDGE_MOVE = 1.0;

    /// @brief Counts the double roundings of the section gain at zero frequency: three adds
    /// in the numerator, two in the denominator and one division, taken twice over for the
    /// fit's own normalisation.
    constexpr double K_DC_GAIN_OPERATIONS = 12.0;

    /// @brief Holds a named target in decibels, a dip between flat bands.
    struct DipRow
    {
            const char* name;
            Decibels decibels;
    };

    /// @brief Names a row for gtest.
    template <typename Row>
    std::string row_name(const ::testing::TestParamInfo<Row>& info)
    {
        return info.param.name;
    }

    /// @brief Returns true when the section's poles lie inside the stability triangle.
    bool inside_stability_triangle(const std::array<double, 5>& section)
    {
        const double a1 = section[3];
        const double a2 = section[4];
        return std::fabs(a2) < 1.0 && std::fabs(a1) < 1.0 + a2;
    }

    /// @brief Returns a section's gain at zero frequency.
    double dc_gain(const std::array<double, 5>& section)
    {
        return (section[0] + section[1] + section[2]) / (1.0 + section[3] + section[4]);
    }

    /// @brief Returns the size of the terms behind dc_gain over its denominator, the gauge of
    /// its rounding error.
    double dc_gain_gauge(const std::array<double, 5>& section)
    {
        return (std::fabs(section[0]) + std::fabs(section[1]) + std::fabs(section[2])) /
               std::fabs(1.0 + section[3] + section[4]);
    }

    /// @brief Returns the largest absolute difference between two cascades' coefficients.
    double largest_coefficient_move(const BandResponseFit::Coefficients& a,
                                    const BandResponseFit::Coefficients& b)
    {
        double largest = 0.0;
        for (std::size_t index = 0; index < 5; ++index)
        {
            largest = std::max(largest, std::fabs(a.section_a[index] - b.section_a[index]));
            largest = std::max(largest, std::fabs(a.section_b[index] - b.section_b[index]));
        }
        return largest;
    }

    /// @brief Names a target by material, rate, order and metres of air.
    std::string label_of(int material, double rate, double order, double metres)
    {
        return "material " + std::to_string(material) + " order " + std::to_string(order) + " rate " +
               std::to_string(rate) + " metres " + std::to_string(metres);
    }

    /// @brief Calls the visitor with every material but the doorway, raised to orders one to
    /// four, at four sample rates, with no air in the path.
    void for_every_material_target(const TargetVisitor& visitor)
    {
        for (std::uint8_t id = 0; id < SushiDSP::room::K_MATERIAL_COUNT; ++id)
        {
            const auto material_id = static_cast<MaterialId>(id);
            if (material_id == MaterialId::OpenDoorway)
            {
                continue;
            }
            for (const double rate : {44100.0, 48000.0, 96000.0, 192000.0})
            {
                for (const double order : {1.0, 2.0, 3.0, 4.0})
                {
                    visitor(SushiDSP::room::reflection_factor(material_id).powered(order), rate,
                            label_of(int(id), rate, order, 0.0));
                }
            }
        }
    }

    /// @brief Calls the visitor with every material but the doorway, raised to orders one to
    /// four, behind 0, 5 and 30 m of air, at four sample rates.
    void for_every_target(const TargetVisitor& visitor)
    {
        for (std::uint8_t id = 0; id < SushiDSP::room::K_MATERIAL_COUNT; ++id)
        {
            const auto material_id = static_cast<MaterialId>(id);
            if (material_id == MaterialId::OpenDoorway)
            {
                continue;
            }
            for (const double rate : {44100.0, 48000.0, 96000.0, 192000.0})
            {
                for (const double order : {1.0, 2.0, 3.0, 4.0})
                {
                    for (const double metres : {0.0, 5.0, 30.0})
                    {
                        BandResponse target = SushiDSP::room::reflection_factor(material_id).powered(order);
                        target *= AirAbsorption::over_path(metres);
                        visitor(target, rate, label_of(int(id), rate, order, metres));
                    }
                }
            }
        }
    }

    /// @brief Returns the target with one band raised by the given decibels.
    BandResponse nudged(const BandResponse& target, std::size_t band, double decibels)
    {
        BandResponse result = target;
        result.gain[band] *= static_cast<float>(std::pow(10.0, decibels / 20.0));
        return result;
    }

    /// @brief Expects the fit of @p target to land within K_FIT_BAND_DB at every band centre.
    void expect_fit_clears_one_decibel(const BandResponse& target, double rate, const std::string& label)
    {
        const BandResponseFit::Result result = BandResponseFit::fit(target, rate);
        EXPECT_LE(result.worst_error_db, K_FIT_BAND_DB) << label;
        for (std::size_t band = 0; band < K_BAND_CENTRES_HZ.size(); ++band)
        {
            const double wanted = 20.0 * std::log10(std::max(target.gain[band], 1e-9f));
            const double got = BandResponseFit::magnitude_db(result.coefficients, K_BAND_CENTRES_HZ[band], rate);
            EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(got, wanted, K_FIT_BAND_DB, 1.0)))
                << label << " band " << band;
        }
    }

    /// @brief Expects both sections of the fit of @p target to keep their poles inside the unit
    /// circle.
    void expect_fit_is_stable(const BandResponse& target, double rate, const std::string& label)
    {
        const BandResponseFit::Result result = BandResponseFit::fit(target, rate);
        EXPECT_TRUE(inside_stability_triangle(result.coefficients.section_a)) << label;
        EXPECT_TRUE(inside_stability_triangle(result.coefficients.section_b)) << label;
    }

    /// @brief Expects the second section of the fit of @p target to have unity gain at zero
    /// frequency, which is the whole gain sitting in the first section.
    void expect_second_section_is_unity_at_dc(const BandResponse& target, double rate, const std::string& label)
    {
        const BandResponseFit::Result result = BandResponseFit::fit(target, rate);
        const double bound = K_DC_GAIN_OPERATIONS * TestSupport::unit_roundoff<double>() *
                             dc_gain_gauge(result.coefficients.section_b);
        EXPECT_TRUE(TestSupport::is_within(
            TestSupport::measure_error(dc_gain(result.coefficients.section_b), 1.0, bound)))
            << label;
    }

    /// @brief Expects a nudge of 0.05 dB or of 0.5 dB to one band to move the coefficients less
    /// than the bounds above.
    void expect_fit_moves_continuously(const BandResponse& target, double rate, const std::string& label)
    {
        const BandResponseFit::Result base = BandResponseFit::fit(target, rate);
        for (std::size_t band = 0; band < SushiDSP::room::K_BAND_COUNT; ++band)
        {
            const BandResponseFit::Result small = BandResponseFit::fit(nudged(target, band, 0.05), rate);
            EXPECT_LT(largest_coefficient_move(base.coefficients, small.coefficients), K_SMALL_NUDGE_MOVE)
                << label << " band " << band << " nudged 0.05 dB";
            const BandResponseFit::Result large = BandResponseFit::fit(nudged(target, band, 0.5), rate);
            EXPECT_LT(largest_coefficient_move(base.coefficients, large.coefficients), K_LARGE_NUDGE_MOVE)
                << label << " band " << band << " nudged 0.5 dB";
        }
    }

    /// @brief Expects the magnitude of @p coefficients at each centre to sit within the dip band
    /// of the matching entry of @p decibels.
    void expect_magnitudes_near(const BandResponseFit::Coefficients& coefficients, const Decibels& decibels)
    {
        for (std::size_t band = 0; band < K_BAND_CENTRES_HZ.size(); ++band)
        {
            const double got = BandResponseFit::magnitude_db(coefficients, K_BAND_CENTRES_HZ[band], 48000.0);
            EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(got, decibels[band], K_DIP_BAND_DB, 1.0)))
                << "band " << band;
        }
    }

    /// @brief Holds what the fits of a sweep reported about their steps.
    struct StepSurvey
    {
            /// @brief Stores the largest step count of a fit that hit the cap, zero for none.
            std::uint32_t cap_steps = 0;

            /// @brief Stores how many fits hit the cap with a count different from the first.
            std::uint32_t disagreeing_caps = 0;

            /// @brief Stores the largest step count over all fits.
            std::uint32_t most_steps = 0;
    };

    /// @brief Fits every target of the sweep and records the steps each one reports.
    StepSurvey survey_steps()
    {
        StepSurvey survey;
        for_every_target(
            [&survey](const BandResponse& target, double rate, const std::string&)
            {
                const BandResponseFit::Result result = BandResponseFit::fit(target, rate);
                survey.most_steps = std::max(survey.most_steps, result.steps);
                if (result.ending == BandResponseFit::Ending::StepCap)
                {
                    survey.disagreeing_caps += (survey.cap_steps != 0 && survey.cap_steps != result.steps) ? 1u : 0u;
                    survey.cap_steps = result.steps;
                }
            });
        return survey;
    }

    class Small_BandResponseFitDip : public ::testing::TestWithParam<DipRow>
    {
    };
} // namespace

TEST(Small_BandResponseFit, BandResponseFitClearsOneDecibelForEveryMaterialToEveryOrder)
{
    for_every_material_target(expect_fit_clears_one_decibel);
}

TEST(Small_BandResponseFit, BandResponseFitReproducesAFlatTargetToAHundredthOfADecibel)
{
    const BandResponseFit::Result result = BandResponseFit::fit(BandResponse::unity(), 48000.0);

    EXPECT_LT(result.worst_error_db, K_FLAT_BAND_DB);
    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(
        BandResponseFit::magnitude_db(result.coefficients, 1000.0, 48000.0), 0.0, K_FLAT_BAND_DB, 1.0)));
}

TEST(Small_BandResponseFit, BandResponseFitKeepsEveryPoleInsideTheUnitCircle)
{
    for_every_material_target(expect_fit_is_stable);
}

TEST(Small_BandResponseFit, BandResponseFitPutsTheWholeGainInTheFirstSection)
{
    for_every_target(expect_second_section_is_unity_at_dc);
}

TEST_P(Small_BandResponseFitDip, BandResponseFitReachesADipBetweenFlatBands)
{
    // Each section has a peak, since two shelves cannot dip. See tests/nodes/room/README.md.
    const Decibels decibels = GetParam().decibels;

    const BandResponseFit::Result result = BandResponseFit::fit(BandResponse::from_decibels(decibels), 48000.0);

    EXPECT_LE(result.worst_error_db, K_DIP_BAND_DB);
    expect_magnitudes_near(result.coefficients, decibels);
    EXPECT_TRUE(inside_stability_triangle(result.coefficients.section_a));
    EXPECT_TRUE(inside_stability_triangle(result.coefficients.section_b));
}

INSTANTIATE_TEST_SUITE_P(
    Dips, Small_BandResponseFitDip,
    ::testing::Values(DipRow{"AtOneKilohertz", {0.0f, 0.0f, 0.0f, -8.0f, 0.0f, 0.0f}},
                      DipRow{"AtTwoKilohertz", {0.0f, 0.0f, 0.0f, 0.0f, -20.0f, 0.0f}},
                      DipRow{"OfAGarageWall", {-0.05f, -0.19f, -0.76f, -3.11f, -11.50f, -3.49f}}),
    row_name<DipRow>);

TEST(Small_BandResponseFit, BandResponseFitMovesCoefficientsContinuouslyWithTheTarget)
{
    for_every_target(expect_fit_moves_continuously);
}

TEST(Small_BandResponseFit, BandResponseFitGivesTheSameFitAndTheSameStepsOnASecondCall)
{
    const BandResponse carpet = SushiDSP::room::reflection_factor(MaterialId::HeavyCarpetOnConcrete);

    const BandResponseFit::Result first = BandResponseFit::fit(carpet, 48000.0);
    const BandResponseFit::Result again = BandResponseFit::fit(carpet, 48000.0);

    EXPECT_EQ(again.steps, first.steps);
    EXPECT_EQ(again.ending, first.ending);
    EXPECT_EQ(again.coefficients.section_a, first.coefficients.section_a);
    EXPECT_EQ(again.coefficients.section_b, first.coefficients.section_b);
}

/*
 * The step cap is private to band_response_fit.cpp, so the relation is stated without its value:
 * every fit that stops on the cap reports the same count, and no fit reports more.
 */
TEST(Small_BandResponseFit, BandResponseFitReportsOneStepCountForEveryFitThatStopsOnTheCap)
{
    const StepSurvey survey = survey_steps();

    EXPECT_EQ(survey.disagreeing_caps, 0u);
}

TEST(Small_BandResponseFit, BandResponseFitNeverReportsMoreStepsThanTheCap)
{
    const StepSurvey survey = survey_steps();

    EXPECT_TRUE(survey.cap_steps == 0u || survey.most_steps <= survey.cap_steps);
}

TEST(Small_BandResponseFit, BandResponseFitOfAFlatTargetDoesNotRunToTheStepCap)
{
    const BandResponseFit::Result result = BandResponseFit::fit(BandResponse::unity(), 48000.0);

    EXPECT_NE(result.ending, BandResponseFit::Ending::StepCap);
}
