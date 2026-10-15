/**************************************************************************/
/* test_early_tap_set.cpp                                                 */
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
 * @file test_early_tap_set.cpp
 * @brief Tests the early tap record and the capacity of an early tap set.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/nodes/room/early_tap_set.hpp>

#include <array>

TEST(Small_EarlyTapSet, HoldsAtMostOneHundredTwentyEightTaps)
{
    EXPECT_EQ(SushiDSP::room::EarlyTapSet::K_MAX_EARLY_TAPS, 128u);
}

TEST(Small_EarlyTap, KeepsItsDelayGainAndFirstSection)
{
    const SushiDSP::room::EarlyTap tap{24.0f, 0.5f, {{1.0, 0.0, 0.0, 0.0, 0.0}}};
    EXPECT_FLOAT_EQ(tap.delay_frames, 24.0f);
    EXPECT_FLOAT_EQ(tap.gain, 0.5f);
    EXPECT_EQ(tap.section_a[0], 1.0);
}
