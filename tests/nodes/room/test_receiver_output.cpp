/**************************************************************************/
/* test_receiver_output.cpp                                               */
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
 * @file test_receiver_output.cpp
 * @brief Tests that a head holder keeps the HRTF source it was given.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/nodes/room/receiver_output.hpp>
#include <SushiDSP/nodes/room/room_host.hpp>

TEST(Small_ReceiverOutput, HeadHolderKeepsTheSourceItWasGiven)
{
    SushiDSP::Room::HeadHolder holder;
    const auto before = holder.source();
    holder.set_source(before);
    EXPECT_EQ(holder.source(), before);
}
