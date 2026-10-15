/**************************************************************************/
/* test_direct_path.cpp                                                   */
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
 * @file test_direct_path.cpp
 * @brief Tests the bank count of the direct path and that it resets after preparation.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/nodes/room/direct_path.hpp>

#include <cstdint>

TEST(Small_DirectPath, BankCountMatchesTheEarlyReflectionRenderer)
{
    EXPECT_EQ(SushiDSP::room::DirectPath::K_BANKS,
              SushiDSP::room::EarlyReflectionRenderer::K_BANK_COUNT);
}

TEST(Small_DirectPath, ClearingAPreparedBankIsSafe)
{
    SushiDSP::room::DirectPath path;
    path.prepare(16u, 64u);
    path.reset();
    path.clear_bank(0u);
    SUCCEED();
}
