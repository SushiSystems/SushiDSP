/**************************************************************************/
/* test_delay_ring.cpp                                                    */
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
 * @file test_delay_ring.cpp
 * @brief Tests that a delay ring holds silence after reset and reads back zero from it.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/nodes/room/delay_ring.hpp>

#include <array>

TEST(DelayRing, ReadAfterResetGivesSilence)
{
    SushiDSP::room::DelayRing ring;
    ring.prepare(64u, 16u);
    ring.reset();
    std::array<float, 8> output{};
    ring.read(4.0, 1.0f, output.data(), 8u);
    for (float sample : output)
    {
        EXPECT_FLOAT_EQ(sample, 0.0f);
    }
}

TEST(DelayRing, WriteFollowedByResetLeavesNoEcho)
{
    SushiDSP::room::DelayRing ring;
    ring.prepare(64u, 16u);
    ring.reset();
    std::array<float, 16> block{};
    block.fill(0.75f);
    ring.write(block.data(), 16u);
    ring.reset();
    std::array<float, 8> output{};
    ring.read(0.0, 1.0f, output.data(), 8u);
    for (float sample : output)
    {
        EXPECT_FLOAT_EQ(sample, 0.0f);
    }
}
