/**************************************************************************/
/* test_pair_stage.cpp                                                    */
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
 * @file test_pair_stage.cpp
 * @brief Tests that a pair stage is idle after reset and starts with no live render set.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/nodes/room/pair_stage.hpp>

#include <cstdint>

TEST(PairStage, IsNotLiveAfterReset)
{
    SushiDSP::Room::PairStage stage;
    stage.reset();
    EXPECT_FALSE(stage.live());
}

TEST(PairStage, HasTwoChannels)
{
    EXPECT_EQ(SushiDSP::Room::PairStage::K_CHANNELS, 2u);
}
