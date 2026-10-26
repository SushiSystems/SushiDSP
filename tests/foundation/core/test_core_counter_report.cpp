/**************************************************************************/
/* test_core_counter_report.cpp                                           */
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
 * @file test_core_counter_report.cpp
 * @brief Tests that the core report writes the graph's and the nodes' counters as one WARN.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/core/core_counter_report.hpp>
#include <SushiDSP/foundation/log_capture/log_capture.hpp>

#include <chrono>
#include <string>
#include <vector>

TEST(Small_CoreCounterReport, AClampedSampleIsWrittenAsOneWarnUnderTheCoreCategory)
{
    SushiDSP::CoreCounterReport report;
    const SushiDSP::RtCounterReporter::Clock::time_point start{};
    report.poll(start);
    SushiDSP::node_counters().output_clamped.increment(7U);
    SushiDSP::LogCapture capture;

    report.poll(start + std::chrono::seconds(5));

    const std::vector<SushiDSP::CapturedLogLine> lines = capture.lines();
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0].level, SushiLog::LogLevel::Warn);
    EXPECT_EQ(lines[0].category, "core");
    EXPECT_EQ(lines[0].message, "7 output_clamped in 5 s");
}

TEST(Small_CoreCounterReport, ADroppedParameterChangeOfAGraphIsReported)
{
    SushiDSP::CoreCounterReport report;
    const SushiDSP::RtCounterReporter::Clock::time_point start{};
    report.poll(start);
    SushiDSP::LogCapture capture;
    SushiDSP::RigGraph graph(64, 4);
    graph.compile(SushiDSP::PrepareInfo{48000.0, 64, 1});
    static_cast<void>(graph.push_parameter_change(SushiDSP::ParameterChange{5, 0, 1.0f}));
    std::vector<float> output(64, 0.0f);
    float* outputs[1] = {output.data()};
    SushiDSP::ProcessContext context{outputs, nullptr, 64, 0.0};
    graph.process(context);

    report.poll(start + std::chrono::seconds(5));

    const std::vector<SushiDSP::CapturedLogLine> lines = capture.lines();
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_NE(lines[0].message.find("missing_node_change"), std::string::npos)
        << lines[0].message;
}

TEST(Small_CoreCounterReport, NothingMovedWritesNothing)
{
    SushiDSP::CoreCounterReport report;
    const SushiDSP::RtCounterReporter::Clock::time_point start{};
    report.poll(start);
    SushiDSP::LogCapture capture;

    report.poll(start + std::chrono::seconds(5));

    EXPECT_TRUE(capture.lines().empty());
}
