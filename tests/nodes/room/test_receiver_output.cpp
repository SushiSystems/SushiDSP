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
