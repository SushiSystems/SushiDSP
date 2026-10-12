/**************************************************************************/
/* test_musical_time_text.cpp                                             */
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
 * @file test_musical_time_text.cpp
 * @brief Tests the bar, beat and sixteenth text of MusicalTimeText.
 * @author Mustafa Garip
 */

#include <cstdint>

#include <gtest/gtest.h>

#include "SushiDSP/capture/musical_clock.hpp"

#include "ui/musical_time_text.hpp"

namespace
{
    /// @brief Builds the grid every case reads: 120 BPM in 4/4 at 48 kHz, where a beat is
    /// 24000 frames and a bar is 96000.
    /// @param clock The clock to set up.
    void make_four_four(SushiDSP::MusicalClock& clock)
    {
        clock.set_tempo(120.0);
        clock.set_meter(4u, 4u);
        clock.set_sample_rate(48000.0);
    }
} // namespace

TEST(Unit_Dsp, MusicalTimeTextReadsFrameZeroAsBarOneBeatOneSixteenthOne)
{
    SushiDSP::MusicalClock clock;
    make_four_four(clock);

    EXPECT_EQ(ui::bar_beat_sixteenth_text(0u, clock), "1 · 1 · 1");
}

TEST(Unit_Dsp, MusicalTimeTextReadsBarOneHundredAtItsExactFrame)
{
    SushiDSP::MusicalClock clock;
    make_four_four(clock);

    ASSERT_EQ(clock.frame_of(100u, 1u), 9504000u);
    EXPECT_EQ(ui::bar_beat_sixteenth_text(9504000u, clock), "100 · 1 · 1");
}

TEST(Unit_Dsp, MusicalTimeTextCountsTheSixteenthInsideABeat)
{
    SushiDSP::MusicalClock clock;
    make_four_four(clock);

    EXPECT_EQ(ui::bar_beat_sixteenth_text(6000u, clock), "1 · 1 · 2");
    EXPECT_EQ(ui::bar_beat_sixteenth_text(12000u, clock), "1 · 1 · 3");
    EXPECT_EQ(ui::bar_beat_sixteenth_text(42000u, clock), "1 · 2 · 4");
    EXPECT_EQ(ui::bar_beat_sixteenth_text(96000u, clock), "2 · 1 · 1");
}

TEST(Unit_Dsp, MusicalTimeTextLabelsAClipWithItsBarAndBeat)
{
    SushiDSP::MusicalClock clock;
    make_four_four(clock);

    EXPECT_EQ(ui::bar_beat_text(0u, clock), "1.1");
    EXPECT_EQ(ui::bar_beat_text(768000u, clock), "9.1");
    EXPECT_EQ(ui::bar_beat_text(24000u, clock), "1.2");
}
