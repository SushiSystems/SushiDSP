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

#include <SushiDSP/foundation/core/parameter_table.hpp>

#include <array>
#include <cstdint>

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

TEST(ParameterTable, CountIsTheNumberOfRows)
{
    const SushiDSP::ParameterTable<Pedal, 2> table(K_ROWS);
    EXPECT_EQ(table.count(), 2u);
}

TEST(ParameterTable, WritesAndReadsARowByItsIdentifier)
{
    const SushiDSP::ParameterTable<Pedal, 2> table(K_ROWS);
    Pedal pedal;
    table.write(pedal, "tone", 0.75f);
    EXPECT_FLOAT_EQ(pedal.tone, 0.75f);
    EXPECT_FLOAT_EQ(table.read(pedal, "tone"), 0.75f);
}

TEST(ParameterTable, WritesARowByItsIndex)
{
    const SushiDSP::ParameterTable<Pedal, 2> table(K_ROWS);
    Pedal pedal;
    table.write(pedal, static_cast<std::uint16_t>(0), 4.0f);
    EXPECT_FLOAT_EQ(pedal.gain, 4.0f);
}

TEST(ParameterTable, AnUnknownIdentifierChangesNothingAndReadsZero)
{
    const SushiDSP::ParameterTable<Pedal, 2> table(K_ROWS);
    Pedal pedal;
    pedal.gain = 3.0f;
    table.write(pedal, "missing", 9.0f);
    EXPECT_FLOAT_EQ(pedal.gain, 3.0f);
    EXPECT_FLOAT_EQ(table.read(pedal, "missing"), 0.0f);
}

TEST(ParameterTable, DescriptorsFollowTheRowOrder)
{
    const SushiDSP::ParameterTable<Pedal, 2> table(K_ROWS);
    const SushiDSP::ParameterDescriptor* descriptors = table.descriptors();
    EXPECT_STREQ(descriptors[0].id, "gain");
    EXPECT_STREQ(descriptors[1].id, "tone");
}
