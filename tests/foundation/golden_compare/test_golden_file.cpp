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
 * @file test_golden_file.cpp
 * @brief Tests the stimulus and the residual rule of the golden-file comparison.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/golden_compare/golden_file.hpp>
#include <SushiDSP/foundation/log/capturing_log_sink.hpp>
#include <SushiDSP/foundation/log/scoped_log_sink.hpp>

#include <cmath>
#include <vector>

TEST(Small_GoldenFile, StimulusHasOneSecondOfSamples)
{
    EXPECT_EQ(SushiDSP::golden::stimulus(48000.0).size(), 48000u);
    EXPECT_EQ(SushiDSP::golden::stimulus(96000.0).size(), 96000u);
}

TEST(Small_GoldenFile, StimulusIsDeterministic)
{
    EXPECT_EQ(SushiDSP::golden::stimulus(4800.0), SushiDSP::golden::stimulus(4800.0));
}

TEST(Small_GoldenFile, IdenticalSignalsGiveTheFloorResidual)
{
    const std::vector<float> signal = SushiDSP::golden::stimulus(4800.0);
    EXPECT_EQ(SushiDSP::golden::residual_db(signal, signal), -200.0);
}

TEST(Small_GoldenFile, HalfAmplitudeGivesMinusSixDecibels)
{
    const std::vector<float> signal = SushiDSP::golden::stimulus(4800.0);
    std::vector<float> half(signal.size());
    for (std::size_t i = 0; i < signal.size(); ++i)
    {
        half[i] = 0.5f * signal[i];
    }
    EXPECT_NEAR(SushiDSP::golden::residual_db(signal, half), 10.0 * std::log10(0.25), 1e-3);
}

TEST(Small_GoldenFile, SilentReferenceGivesTheFloorResidual)
{
    const std::vector<float> silence(100, 0.0f);
    const std::vector<float> other(100, 0.1f);
    EXPECT_EQ(SushiDSP::golden::residual_db(silence, other), -200.0);
}

TEST(Small_GoldenFile, MissingGoldenFileIsLoggedAsAnErrorAndReadsAsZeroDecibels)
{
    const auto capture = std::make_shared<SushiDSP::CapturingLogSink>();
    double residual = -1.0;
    {
        const SushiDSP::ScopedLogSink guard(SushiDSP::default_logger(), capture);
        residual = SushiDSP::golden::compare_or_regenerate("no_such_folder", "absent",
                                                           SushiDSP::golden::stimulus(4800.0));
    }

    EXPECT_EQ(residual, 0.0);
    const std::vector<SushiDSP::CapturedLogLine> lines = capture->lines();
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0].level, SushiDSP::LogLevel::Error);
    EXPECT_EQ(lines[0].category, "golden_compare");
    EXPECT_EQ(lines[0].message,
              "cannot open golden file no_such_folder/absent.f32; reporting a 0 dB residual");
}