/**************************************************************************/
/* test_stage_limits.cpp                                                  */
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
 * @file test_stage_limits.cpp
 * @brief Tests the image order and the early-delay ceiling, which scales with the sample rate.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/nodes/room/stage_limits.hpp>

#include <cstdint>

TEST(StageLimits, ImageOrderAndItdMarginAreTheDocumentedValues)
{
    EXPECT_EQ(SushiDSP::Room::K_IMAGE_ORDER, 4u);
    EXPECT_EQ(SushiDSP::Room::K_ITD_MARGIN_FRAMES, 128u);
}

TEST(StageLimits, EarlyDelayCeilingScalesWithTheSampleRate)
{
    const std::uint32_t at_48k = SushiDSP::Room::max_early_delay_frames(48000.0);
    const std::uint32_t at_96k = SushiDSP::Room::max_early_delay_frames(96000.0);
    EXPECT_GT(at_48k, 0u);
    EXPECT_NEAR(static_cast<double>(at_96k), 2.0 * static_cast<double>(at_48k), 1.0);
}
