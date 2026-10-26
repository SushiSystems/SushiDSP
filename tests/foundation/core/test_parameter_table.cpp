/**************************************************************************/
/* test_parameter_table.cpp                                               */
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
 * @file test_parameter_table.cpp
 * @brief Tests that a parameter table writes and reads each row by its identifier or index.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/allocation_trap/allocation_trap.hpp>
#include <SushiDSP/foundation/core/parameter_table.hpp>
#include <SushiDSP/foundation/log_capture/log_capture.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace
{
    struct Pedal
    {
        float gain = 0.0f;
        float tone = 0.0f;
    };

    const std::array<SushiDSP::ParameterBinding<Pedal>, 2> K_ROWS{{
        SushiDSP::ParameterBinding<Pedal>{
            {"gain", "Gain", 0.0f, 10.0f, 1.0f, SushiDSP::ParamKind::Linear, ""},
            [](Pedal& pedal, float value) { pedal.gain = value; },
            [](const Pedal& pedal) { return pedal.gain; }},
        SushiDSP::ParameterBinding<Pedal>{
            {"tone", "Tone", 0.0f, 1.0f, 0.5f, SushiDSP::ParamKind::Linear, ""},
            [](Pedal& pedal, float value) { pedal.tone = value; },
            [](const Pedal& pedal) { return pedal.tone; }},
    }};
} // namespace

TEST(Small_ParameterTable, CountIsTheNumberOfRows)
{
    const SushiDSP::ParameterTable<Pedal, 2> table(K_ROWS);
    EXPECT_EQ(table.count(), 2u);
}

TEST(Small_ParameterTable, WritesAndReadsARowByItsIdentifier)
{
    const SushiDSP::ParameterTable<Pedal, 2> table(K_ROWS);
    Pedal pedal;
    table.write(pedal, "tone", 0.75f);
    EXPECT_FLOAT_EQ(pedal.tone, 0.75f);
    EXPECT_FLOAT_EQ(table.read(pedal, "tone"), 0.75f);
}

TEST(Small_ParameterTable, WritesARowByItsIndex)
{
    const SushiDSP::ParameterTable<Pedal, 2> table(K_ROWS);
    Pedal pedal;
    table.write(pedal, static_cast<std::uint16_t>(0), 4.0f);
    EXPECT_FLOAT_EQ(pedal.gain, 4.0f);
}

TEST(Small_ParameterTable, AnUnknownIdentifierChangesNothingAndReadsZero)
{
    const SushiDSP::ParameterTable<Pedal, 2> table(K_ROWS);
    Pedal pedal;
    pedal.gain = 3.0f;
    table.write(pedal, "missing", 9.0f);
    EXPECT_FLOAT_EQ(pedal.gain, 3.0f);
    EXPECT_FLOAT_EQ(table.read(pedal, "missing"), 0.0f);
}

TEST(Small_ParameterTable, DescriptorsFollowTheRowOrder)
{
    const SushiDSP::ParameterTable<Pedal, 2> table(K_ROWS);
    const SushiDSP::ParameterDescriptor* descriptors = table.descriptors();
    EXPECT_STREQ(descriptors[0].id, "gain");
    EXPECT_STREQ(descriptors[1].id, "tone");
}

TEST(Small_ParameterTableLogging, AnUnknownIdentifierWritesOneWarnNamingTheId)
{
    const SushiDSP::ParameterTable<Pedal, 2> table(K_ROWS);
    Pedal pedal;

    SushiDSP::LogCapture capture;
    table.write(pedal, "missing", 9.0f);

    const std::vector<SushiDSP::CapturedLogLine> lines = capture.lines();
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0].level, SushiLog::LogLevel::Warn);
    EXPECT_EQ(lines[0].category, "core");
    EXPECT_NE(lines[0].message.find("'missing'"), std::string::npos) << lines[0].message;
}

TEST(Small_ParameterTableLogging, AKnownIdentifierWritesNothing)
{
    const SushiDSP::ParameterTable<Pedal, 2> table(K_ROWS);
    Pedal pedal;

    SushiDSP::LogCapture capture;
    table.write(pedal, "gain", 2.0f);

    EXPECT_TRUE(capture.lines().empty());
}

TEST(Small_ParameterTableLogging, TheIndexWriteStaysSilentAndAllocationFreeOutOfRange)
{
    const SushiDSP::ParameterTable<Pedal, 2> table(K_ROWS);
    Pedal pedal;

    SushiDSP::LogCapture capture;
    SushiDSP::test::AllocationTrap trap;
    trap.arm();
    table.write(pedal, static_cast<std::uint16_t>(7), 1.0f);
    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
    EXPECT_TRUE(capture.lines().empty());
}
