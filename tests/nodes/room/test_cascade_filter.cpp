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
/* Licensed under the Apache License, Version 2.0 (the "License"); you    */
/* may not use this file except in compliance with the License. You may   */
/* obtain a copy of the License at                                        */
/*                                                                        */
/*     https://www.apache.org/licenses/LICENSE-2.0                        */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
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

    const float output = filter.process_sample(0.0f);

    EXPECT_TRUE(std::isfinite(output));
    EXPECT_EQ(output, 0.0f);
}
