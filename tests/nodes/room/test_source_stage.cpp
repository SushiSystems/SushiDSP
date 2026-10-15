/**************************************************************************/
/* test_source_stage.cpp                                                  */
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
 * @file test_source_stage.cpp
 * @brief Tests that a source stage is idle after reset and keeps its two banks.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/nodes/room/source_stage.hpp>

#include <cstdint>

TEST(Small_SourceStage, IsNotLiveAfterReset)
{
    SushiDSP::Room::SourceStage stage;
    stage.reset();
    EXPECT_FALSE(stage.live());
}

TEST(Small_SourceStage, HasTwoBanks)
{
    EXPECT_EQ(SushiDSP::Room::SourceStage::K_BANKS, 2u);
}
