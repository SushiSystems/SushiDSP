/**************************************************************************/
/* test_musical_time_text.cpp                                             */
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

#include "SushiDSP/capture/MusicalClock.hpp"

#include "ui/MusicalTimeText.hpp"

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
}

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
