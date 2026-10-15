/**************************************************************************/
/* test_sine_tone_node.cpp                                                */
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
 * @file test_sine_tone_node.cpp
 * @brief Tests the range, frequency and reset of SineToneNode.
 * @author Mustafa Garip
 */

#include <cmath>
#include <vector>

#include <gtest/gtest.h>

#include "sine_tone_node.hpp"

TEST(Small_SineToneNode, SineToneNodeOutputStaysWithinUnitRange)
{
    SushiDSP::SineToneNode node(440.0f);
    SushiDSP::PrepareInfo info{48000.0, 256, 1};
    node.prepare(info);

    std::vector<float> output_buffer(256, 0.0f);
    float* output_channels[1] = {output_buffer.data()};
    SushiDSP::ProcessContext context{output_channels, nullptr, 256, 0.0};

    node.process(context);

    for (float sample : output_buffer)
    {
        EXPECT_LE(std::abs(sample), 1.0f);
    }
}

TEST(Small_SineToneNode, SineToneNodeProducesExpectedFrequency)
{
    constexpr double sample_rate = 48000.0;
    constexpr float frequency = 1000.0f;
    SushiDSP::SineToneNode node(frequency);
    SushiDSP::PrepareInfo info{sample_rate, 480, 1};
    node.prepare(info);

    std::vector<float> output_buffer(480, 0.0f);
    float* output_channels[1] = {output_buffer.data()};
    SushiDSP::ProcessContext context{output_channels, nullptr, 480, 0.0};

    node.process(context);

    int zero_crossings = 0;
    for (std::size_t i = 1; i < output_buffer.size(); ++i)
    {
        if ((output_buffer[i - 1] < 0.0f) != (output_buffer[i] < 0.0f))
        {
            ++zero_crossings;
        }
    }

    const double expected_crossings = 2.0 * frequency * (480.0 / sample_rate);
    EXPECT_NEAR(zero_crossings, expected_crossings, 2.0);
}

TEST(Small_SineToneNode, SineToneNodeResetRestartsPhaseFromZero)
{
    SushiDSP::SineToneNode node(440.0f);
    SushiDSP::PrepareInfo info{48000.0, 8, 1};
    node.prepare(info);

    std::vector<float> first_buffer(8, 0.0f);
    float* first_channels[1] = {first_buffer.data()};
    SushiDSP::ProcessContext first_context{first_channels, nullptr, 8, 0.0};
    node.process(first_context);

    node.reset();

    std::vector<float> second_buffer(8, 0.0f);
    float* second_channels[1] = {second_buffer.data()};
    SushiDSP::ProcessContext second_context{second_channels, nullptr, 8, 0.0};
    node.process(second_context);

    for (std::size_t i = 0; i < first_buffer.size(); ++i)
    {
        EXPECT_NEAR(first_buffer[i], second_buffer[i], 1e-5f);
    }
}
