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
 * @brief Tests that the audio-thread paths of Dd3Delay perform no heap allocation.
 * @author Mustafa Garip
 */

// _USE_MATH_DEFINES precedes every include so that MSVC's <cmath> declares M_PI.
#define _USE_MATH_DEFINES

#include <cmath>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/allocation_trap/allocation_trap.hpp>
#include <SushiDSP/nodes/dd3/dd3_delay.hpp>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

TEST(Dd3Delay, ProcessPerformsNoHeapAllocation)
{
    // Both MODE changes below move N and pay the std::fill. See tests/nodes/dd3/test_dd3_delay/README.md.
    SushiDSP::Dd3Delay node;
    node.prepare(SushiDSP::PrepareInfo{48000.0, 256, 1});
    node.set_time(0.4f);
    node.set_feedback(0.45f);
    node.set_level(0.5f);

    std::vector<float> input(256);
    for (std::size_t i = 0; i < input.size(); ++i)
    {
        input[i] = 0.3f * std::sin(2.0f * static_cast<float>(M_PI) * 220.0f * i / 48000.0f);
    }
    std::vector<float> output(256, 0.0f);
    const float* input_channels[1] = {input.data()};
    float* output_channels[1] = {output.data()};
    SushiDSP::ProcessContext context{output_channels, input_channels, 256, 0.0};

    const std::uint16_t mode_index = 3u;
    const float long_mode_value =
        static_cast<float>(static_cast<int>(SushiDSP::dd3::Dd3Mode::Long));

    SushiDSP::test::AllocationTrap trap;
    trap.arm();

    node.process(context);
    node.set_parameter_for_audio_thread(mode_index, long_mode_value);
    node.process(context);
    node.set_mode(SushiDSP::dd3::Dd3Mode::Short);
    node.process(context);

    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
    for (std::size_t i = 0; i < output.size(); ++i)
    {
        EXPECT_TRUE(std::isfinite(output[i]));
    }
}
