/****************************************************************/
/* test_transport.cpp                                           */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

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
    transport.set_speed(0.6);
    for (std::uint32_t block = 0; block < 1000u; ++block) { transport.advance(64u); }
    EXPECT_EQ(transport.position_frames(), 38400u);   // exactly 0.6 * 64000, no drift
}

TEST(Unit_Dsp, TransportHoldsTheSpeedExactlyOverAHundredThousandBlocks)
{
    SushiDSP::Transport transport;
    transport.play();
    transport.set_speed(1.3);
    for (std::uint32_t block = 0; block < 100000u; ++block) { transport.advance(256u); }
    EXPECT_EQ(transport.position_frames(), 33280000u);   // 1.3 * 25600000, to the frame
}

TEST(Unit_Dsp, TransportWrapsTheLoopOnTheMusicalPositionAtASpeed)
{
    SushiDSP::Transport transport;
    transport.play();
    transport.set_loop(1000u, 5000u);
    transport.seek(1000u);
    transport.set_speed(1.5);
    for (std::uint32_t block = 0; block < 11u; ++block) { transport.advance(256u); }
    EXPECT_EQ(transport.position_frames(), 1224u);   // 1000 + (11 * 384) % 4000
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
