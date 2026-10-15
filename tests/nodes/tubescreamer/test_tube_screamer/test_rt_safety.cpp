/**************************************************************************/
/* test_rt_safety.cpp                                                     */
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
 * @file test_rt_safety.cpp
 * @brief Tests that the audio-thread paths of TubeScreamer perform no heap allocation.
 * @author Mustafa Garip
 */

// _USE_MATH_DEFINES precedes every include so that MSVC's <cmath> declares M_PI.
#define _USE_MATH_DEFINES

#include <cmath>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/allocation_trap/allocation_trap.hpp>
#include <SushiDSP/nodes/tubescreamer/tube_screamer.hpp>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

TEST(Small_TubeScreamer, ProcessPerformsNoHeapAllocation)
{
    SushiDSP::TubeScreamer node;
    SushiDSP::PrepareInfo info{48000.0, 256, 1};
    node.prepare(info);
    node.set_drive(0.6f);
    node.set_tone(0.4f);
    node.set_level(0.9f);

    std::vector<float> input(256);
    for (std::size_t i = 0; i < input.size(); ++i)
    {
        input[i] = 0.3f * std::sin(2.0f * static_cast<float>(M_PI) * 220.0f * i / 48000.0f);
    }
    std::vector<float> output(256, 0.0f);
    const float* input_channels[1] = {input.data()};
    float* output_channels[1] = {output.data()};

    SushiDSP::test::AllocationTrap trap;
    trap.arm();

    // A moved knob re-derives the matrices inside process(). See tests/nodes/tubescreamer/test_tube_screamer/README.md.
    for (int block = 0; block < 1000; ++block)
    {
        const float position = static_cast<float>(block) / 999.0f;
        node.set_drive(position);
        node.set_tone(1.0f - position);
        SushiDSP::ProcessContext context{output_channels, input_channels, 256, 0.0};
        node.process(context);
    }

    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
}
