/**************************************************************************/
/* test_transport.cpp                                                     */
/**************************************************************************/
/*                         This file is part of:                          */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                        https://sushisystems.io                         */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License");        */
/* you may not use this file except in compliance with the License.       */
/* You may obtain a copy of the License at                                */
/*                                                                        */
/*     http://www.apache.org/licenses/LICENSE-2.0                         */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

#include <cstdint>

#include <gtest/gtest.h>

#include "SushiDSP/capture/Transport.hpp"

TEST(Unit_Dsp, TransportAtHalfSpeedAdvancesHalfAsFar)
{
    SushiDSP::Transport transport;
    transport.play();
    transport.set_speed(0.5);
    for (std::uint32_t block = 0; block < 100u; ++block) { transport.advance(256u); }
    EXPECT_EQ(transport.position_frames(), 12800u);
}

TEST(Unit_Dsp, TransportCarriesTheSpeedFractionAcrossBlocks)
{
    SushiDSP::Transport transport;
    transport.play();
    transport.set_speed(0.3);
    for (std::uint32_t block = 0; block < 1000u; ++block) { transport.advance(64u); }
    EXPECT_EQ(transport.position_frames(), 19200u);   // exactly 0.3 * 64000, no drift
}

TEST(Unit_Dsp, TransportClampsASpeedOutsideItsRange)
{
    SushiDSP::Transport transport;
    transport.set_speed(10.0);
    EXPECT_DOUBLE_EQ(transport.speed(), SushiDSP::Transport::kMaxSpeed);
    transport.set_speed(0.1);
    EXPECT_DOUBLE_EQ(transport.speed(), SushiDSP::Transport::kMinSpeed);
}

TEST(Unit_Dsp, TransportSeekClearsTheSpeedFraction)
{
    SushiDSP::Transport transport;
    transport.play();
    transport.set_speed(0.3);
    transport.advance(64u);            // carries a fraction into speed_remainder_
    transport.seek(1000u);
    transport.advance(0u);
    EXPECT_EQ(transport.position_frames(), 1000u);
}

TEST(Unit_Dsp, TransportAtUnitySpeedAdvancesExactlyAsBefore)
{
    SushiDSP::Transport transport;
    transport.play();
    transport.advance(20u);
    transport.advance(45u);
    transport.advance(100u);
    transport.advance(128u);
    EXPECT_EQ(transport.position_frames(), 293u);
}
