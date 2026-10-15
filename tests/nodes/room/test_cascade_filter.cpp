/**************************************************************************/
/* test_cascade_filter.cpp                                                */
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
 * @file test_cascade_filter.cpp
 * @brief Tests that a default cascade filter passes silence as finite silence.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/nodes/room/cascade_filter.hpp>

#include <cmath>

TEST(Small_CascadeFilter, SilenceStaysFinite)
{
    SushiDSP::Room::CascadeFilter filter;
    for (int i = 0; i < 32; ++i)
    {
        const float output = filter.process_sample(0.0f);
        EXPECT_TRUE(std::isfinite(output));
        EXPECT_FLOAT_EQ(output, 0.0f);
    }
}
