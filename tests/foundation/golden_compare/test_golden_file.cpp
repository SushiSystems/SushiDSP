/**************************************************************************/
/* test_golden_file.cpp                                                   */
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
 * @file test_golden_file.cpp
 * @brief Tests the stimulus, the residual rule and the comparison step of the golden-file comparison.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include "support/tolerance.hpp"

#include <SushiDSP/foundation/golden_compare/golden_file.hpp>
#include <SushiDSP/foundation/golden_compare/in_memory_golden_store.hpp>
#include <SushiDSP/foundation/log_capture/log_capture.hpp>

#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

namespace
{
    /// @brief Holds the sample rate of the short stimulus the cases below use.
    constexpr double K_RATE = 4800.0;

    /// @brief Holds the sample indices the formula of stimulus() is checked at.
    constexpr std::size_t K_INDICES[] = {0, 1, 100, 1234, 4799};

    /// @brief Holds the circle constant for the oracle.
    constexpr double K_TWO_PI = 6.283185307179586;

    /// @brief Holds the decibels in one neper of power ratio, 10 / ln 10.
    constexpr double K_DECIBELS_PER_NEPER = 4.342944819032518;

    /// @brief Returns the formula of the stimulus at sample @p index: two tones, at -12 dBFS and -26 dBFS.
    double stimulus_formula(std::size_t index)
    {
        const double seconds = static_cast<double>(index) / K_RATE;
        return 0.251189 * std::sin(K_TWO_PI * 220.0 * seconds) + 0.0501187 * std::sin(K_TWO_PI * 1700.0 * seconds);
    }

    /// @brief Returns @p signal scaled by one half, which is exact in float.
    std::vector<float> halved(const std::vector<float>& signal)
    {
        std::vector<float> half(signal.size());
        for (std::size_t index = 0; index < signal.size(); ++index)
        {
            half[index] = 0.5f * signal[index];
        }
        return half;
    }
} // namespace

class Small_GoldenFileStimulus : public ::testing::TestWithParam<std::size_t>
{
};

TEST_P(Small_GoldenFileStimulus, StimulusFollowsItsFormulaAtTheSampleIndex)
{
    const std::vector<float> stimulus = SushiDSP::golden::stimulus(K_RATE);
    const std::size_t index = K_INDICES[GetParam()];

    EXPECT_TRUE(TestSupport::is_within(
        TestSupport::measure_relative_error<float>(stimulus[index], stimulus_formula(index), 1)));
}

INSTANTIATE_TEST_SUITE_P(Indices, Small_GoldenFileStimulus, ::testing::Values<std::size_t>(0, 1, 2, 3, 4),
                         [](const ::testing::TestParamInfo<std::size_t>& info)
                         { return "Sample" + std::to_string(K_INDICES[info.param]); });

TEST(Small_GoldenFile, StimulusHasOneSecondOfSamples)
{
    EXPECT_EQ(SushiDSP::golden::stimulus(48000.0).size(), 48000u);
    EXPECT_EQ(SushiDSP::golden::stimulus(96000.0).size(), 96000u);
}

TEST(Small_GoldenFile, IdenticalSignalsGiveTheFloorResidual)
{
    const std::vector<float> signal = SushiDSP::golden::stimulus(K_RATE);
    EXPECT_EQ(SushiDSP::golden::residual_db(signal, signal), -200.0);
}

TEST(Small_GoldenFile, HalfAmplitudeGivesMinusSixDecibels)
{
    // Only the two sums of n squares round: the ratio is within 2 * n * u, and 10 / ln 10 turns that into decibels.
    const std::vector<float> signal = SushiDSP::golden::stimulus(K_RATE);
    const double sum_bound = K_DECIBELS_PER_NEPER * 2.0 * static_cast<double>(signal.size()) *
                             TestSupport::unit_roundoff<double>();
    const double expected = 10.0 * std::log10(0.25);

    const double residual = SushiDSP::golden::residual_db(signal, halved(signal));

    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(residual, expected, sum_bound)));
}

TEST(Small_GoldenFile, SilentReferenceGivesTheFloorResidual)
{
    const std::vector<float> silence(100, 0.0f);
    const std::vector<float> other(100, 0.1f);
    EXPECT_EQ(SushiDSP::golden::residual_db(silence, other), -200.0);
}

TEST(Small_GoldenFile, MissingGoldenFileReadsAsZeroDecibels)
{
    SushiDSP::golden::InMemoryGoldenStore store;
    SushiDSP::LogCapture capture;

    const double residual = SushiDSP::golden::compare_or_regenerate(
        store, "no_such_folder", "absent", SushiDSP::golden::stimulus(K_RATE), SushiDSP::golden::GoldenMode::Compare);

    EXPECT_EQ(residual, 0.0);
}

TEST(Small_GoldenFile, MissingGoldenFileIsLoggedAsAnError)
{
    SushiDSP::golden::InMemoryGoldenStore store;
    SushiDSP::LogCapture capture;

    static_cast<void>(SushiDSP::golden::compare_or_regenerate(
        store, "no_such_folder", "absent", SushiDSP::golden::stimulus(K_RATE), SushiDSP::golden::GoldenMode::Compare));

    const std::vector<SushiDSP::CapturedLogLine> lines = capture.lines();
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0].level, SushiLog::LogLevel::Error);
    EXPECT_EQ(lines[0].category, "golden_compare");
    EXPECT_EQ(lines[0].message,
              "cannot open golden file no_such_folder/absent.f32; reporting a 0 dB residual");
}

TEST(Small_GoldenFile, RegenerationReportsTheNoComparisonResidualSoItNeverPasses)
{
    SushiDSP::golden::InMemoryGoldenStore store;
    SushiDSP::LogCapture capture;

    const double residual = SushiDSP::golden::compare_or_regenerate(
        store, "folder", "regenerated", SushiDSP::golden::stimulus(K_RATE), SushiDSP::golden::GoldenMode::Regenerate);

    EXPECT_EQ(residual, 0.0);
}

TEST(Small_GoldenFile, RegenerationStoresTheRenderedSamples)
{
    SushiDSP::golden::InMemoryGoldenStore store;
    SushiDSP::LogCapture capture;

    static_cast<void>(SushiDSP::golden::compare_or_regenerate(
        store, "folder", "regenerated", SushiDSP::golden::stimulus(K_RATE), SushiDSP::golden::GoldenMode::Regenerate));

    EXPECT_TRUE(store.contains("folder/regenerated.f32"));
}

TEST(Small_GoldenFile, ARegeneratedEntryComparesAsIdentical)
{
    SushiDSP::golden::InMemoryGoldenStore store;
    SushiDSP::LogCapture capture;
    const std::vector<float> rendered = SushiDSP::golden::stimulus(K_RATE);
    static_cast<void>(SushiDSP::golden::compare_or_regenerate(store, "folder", "entry", rendered,
                                                              SushiDSP::golden::GoldenMode::Regenerate));

    const double residual = SushiDSP::golden::compare_or_regenerate(store, "folder", "entry", rendered,
                                                                    SushiDSP::golden::GoldenMode::Compare);

    EXPECT_EQ(residual, -200.0);
}
