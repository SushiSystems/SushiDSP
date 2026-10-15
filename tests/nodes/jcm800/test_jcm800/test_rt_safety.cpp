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
 * @brief Tests that the audio-thread paths of Jcm800 perform no heap allocation.
 * @author Mustafa Garip
 */

// _USE_MATH_DEFINES precedes every include so that MSVC's <cmath> declares M_PI.
#define _USE_MATH_DEFINES

#include <cmath>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/allocation_trap/allocation_trap.hpp>
#include <SushiDSP/nodes/jcm800/jcm800.hpp>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

TEST(Small_Jcm800, ProcessPerformsNoHeapAllocationWithoutCabinet)
{
    // The per-block set_high_shelf() is pure arithmetic. See tests/nodes/jcm800/test_jcm800/README.md.
    SushiDSP::Jcm800 node;
    SushiDSP::PrepareInfo info{48000.0, 256, 1};
    node.prepare(info);
    node.set_preamp(0.8f);
    node.set_master(0.0f);
    node.set_bass(0.6f);
    node.set_middle(0.4f);
    node.set_treble(0.7f);
    node.set_presence(0.9f);

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

    for (int block = 0; block < 500; ++block)
    {
        SushiDSP::ProcessContext context{output_channels, input_channels, 256, 0.0};
        node.process(context);
    }

    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
}
