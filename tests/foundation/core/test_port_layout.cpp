/**************************************************************************/
/* test_port_layout.cpp                                                   */
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
 * @file test_port_layout.cpp
 * @brief Tests the channel counts a port layout declares.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/core/port_layout.hpp>

TEST(Small_PortLayout, KeepsTheInputAndOutputChannelCounts)
{
    const SushiDSP::PortLayout stereo{2u, 2u};
    EXPECT_EQ(stereo.input_channels, 2u);
    EXPECT_EQ(stereo.output_channels, 2u);
}

TEST(Small_PortLayout, AMonoToStereoLayoutHasOneInputAndTwoOutputs)
{
    const SushiDSP::PortLayout pan{1u, 2u};
    EXPECT_EQ(pan.input_channels, 1u);
    EXPECT_EQ(pan.output_channels, 2u);
}
