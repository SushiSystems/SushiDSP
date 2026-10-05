/****************************************************************/
/* test_musical_clock.cpp                                       */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file test_musical_clock.cpp
 * @brief Tests the frame arithmetic of MusicalClock.
 * @author Mustafa Garip
 */

#include <cmath>
#include <cstdint>

#include <gtest/gtest.h>

#include "SushiDSP/capture/MusicalClock.hpp"

TEST(Unit_Dsp, MusicalClockPlacesBarOneHundredAtTheExactFrame)
{
    SushiDSP::MusicalClock clock;
    clock.set_sample_rate(48000.0);
    clock.set_tempo(120.0);
    clock.set_meter(4, 4);
    EXPECT_EQ(clock.frame_of(100, 1), 9504000u);
    const SushiDSP::BarBeat at = clock.position_of(9504000u);
    EXPECT_EQ(at.bar, 100u);
    EXPECT_EQ(at.beat, 1u);
    EXPECT_NEAR(at.fraction, 0.0, 1e-9);
}

TEST(Unit_Dsp, MusicalClockRoundTripsEveryFrameToItsOwnBeat)
{
    for (const double rate : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        for (const double bpm : { 120.0, 137.4 })
        {
            SushiDSP::MusicalClock clock;
            clock.set_sample_rate(rate);
            clock.set_tempo(bpm);
            clock.set_meter(4, 4);
            const std::uint64_t step = clock.beat_step_q32();
            for (std::uint64_t frame = 0; frame < 4000000u; frame += 7919u)
            {
                const SushiDSP::BarBeat at = clock.position_of(frame);
                const std::uint64_t beat_start = clock.frame_of(at.bar, at.beat);
                EXPECT_LE(beat_start, frame);
                EXPECT_LT(frame - beat_start, (step >> 32) + 1u);
            }
        }
    }
}

TEST(Unit_Dsp, MusicalClockCountsTheBeatTheMeterNames)
{
    SushiDSP::MusicalClock clock;
    clock.set_sample_rate(48000.0);
    clock.set_tempo(120.0);
    clock.set_meter(6, 8);
    // A 120 BPM quarter is 24000 frames, so an eighth is 12000 and a 6/8 bar is 72000.
    EXPECT_EQ(clock.beat_step_q32() >> 32, 12000u);
    EXPECT_EQ(clock.frame_of(2, 1), 72000u);
}

TEST(Unit_Dsp, MusicalClockDoesNotAccumulateErrorAcrossFiveMinutes)
{
    SushiDSP::MusicalClock clock;
    clock.set_sample_rate(48000.0);
    clock.set_tempo(137.4);
    clock.set_meter(4, 4);
    const std::uint64_t step = clock.beat_step_q32();
    for (std::uint64_t beat = 0; beat < 687u; ++beat)
    {
        const std::uint64_t expected = (beat * step) >> 32;
        const std::uint64_t bar = beat / 4u + 1u;
        const std::uint32_t in_bar = static_cast<std::uint32_t>(beat % 4u) + 1u;
        EXPECT_EQ(clock.frame_of(bar, in_bar), expected);
    }
}

TEST(Unit_Dsp, MusicalClockPublishesTheStepTheTempoAndMeterName)
{
    SushiDSP::MusicalClock clock;
    clock.set_sample_rate(48000.0);
    clock.set_tempo(120.0);
    clock.set_meter(4, 4);
    // 48000 * 60 / 120 is 24000 frames a quarter, and 24000 * 2^32 is exact in a double.
    EXPECT_EQ(clock.beat_step_q32(), 24000ull << 32);

    clock.set_sample_rate(44100.0);
    clock.set_tempo(90.0);
    // 44100 * 60 / 90 is 29400 frames a quarter, so the fractional half of the word is zero.
    EXPECT_EQ(clock.beat_step_q32(), 29400ull << 32);

    // 48000 * 60 / 137.4 is 20960.698689956 frames a quarter, rounded to the nearest 2^-32.
    clock.set_sample_rate(48000.0);
    clock.set_tempo(137.4);
    EXPECT_EQ(clock.beat_step_q32(), 90025515374672ull);
    EXPECT_EQ(clock.beat_step_q32() >> 32, 20960u);

    // An eighth-note beat is exactly half the quarter. See tests/functional/unit/README.md.
    clock.set_meter(7, 8);
    EXPECT_EQ(clock.beat_step_q32(), 45012757687336ull);
}

TEST(Unit_Dsp, MusicalClockPlacesAnAwkwardMeterAtAHandComputedFrame)
{
    SushiDSP::MusicalClock clock;
    clock.set_sample_rate(48000.0);
    clock.set_tempo(137.4);
    clock.set_meter(7, 8);
    // Bar 41 beat 3 is absolute eighth 282, and (282 * 45012757687336) >> 32 is 2955458.
    EXPECT_EQ(clock.frame_of(41, 3), 2955458u);

    const SushiDSP::BarBeat at = clock.position_of(2955458u + 5000u);
    EXPECT_EQ(at.bar, 41u);
    EXPECT_EQ(at.beat, 3u);
    // The frame sits 4999.484 into a 10480.349 frame beat. See tests/functional/unit/README.md.
    EXPECT_NEAR(at.fraction, 0.4770341666682013, 1e-9);
}

TEST(Unit_Dsp, MusicalClockKeepsBarStartsWithinOneSampleFarDownTheTimeline)
{
    SushiDSP::MusicalClock clock;
    clock.set_sample_rate(48000.0);
    clock.set_tempo(137.4);
    clock.set_meter(4, 4);
    // Every start derives from the absolute beat index. See tests/functional/unit/README.md.
    const double frames_per_beat = 48000.0 * 60.0 / 137.4;
    for (const std::uint64_t bar : { 2ull, 100ull, 1000ull, 20000ull })
    {
        const double ideal = static_cast<double>((bar - 1u) * 4u) * frames_per_beat;
        const double actual = static_cast<double>(clock.frame_of(bar, 1));
        EXPECT_GE(ideal - actual, 0.0);
        EXPECT_LT(ideal - actual, 1.0);
    }
    // Bar 1000 at 137.4 BPM is ideally frame 83758951.965, and the shift floors it.
    EXPECT_EQ(clock.frame_of(1000, 1), 83758951u);
}

TEST(Unit_Dsp, MusicalClockIgnoresATempoMeterOrRateThatCannotBeUsed)
{
    SushiDSP::MusicalClock clock;
    clock.set_sample_rate(48000.0);
    clock.set_tempo(120.0);
    clock.set_meter(4, 4);
    const std::uint64_t step = clock.beat_step_q32();

    clock.set_tempo(0.0);
    clock.set_tempo(-120.0);
    clock.set_tempo(std::nan(""));
    clock.set_sample_rate(0.0);
    clock.set_meter(0, 4);
    clock.set_meter(4, 0);

    EXPECT_EQ(clock.beat_step_q32(), step);
    EXPECT_EQ(clock.beats_per_minute(), 120.0);
    EXPECT_EQ(clock.beats_per_bar(), 4u);
    EXPECT_EQ(clock.beat_unit(), 4u);
    EXPECT_EQ(clock.sample_rate(), 48000.0);
}

TEST(Unit_Dsp, MusicalClockReadsAFrameInsideTheFirstBeat)
{
    SushiDSP::MusicalClock clock;
    clock.set_sample_rate(48000.0);
    clock.set_tempo(120.0);
    clock.set_meter(4, 4);
    const SushiDSP::BarBeat at = clock.position_of(12000u);
    EXPECT_EQ(at.bar, 1u);
    EXPECT_EQ(at.beat, 1u);
    EXPECT_NEAR(at.fraction, 0.5, 1e-12);

    const SushiDSP::BarBeat next = clock.position_of(24000u);
    EXPECT_EQ(next.bar, 1u);
    EXPECT_EQ(next.beat, 2u);
    EXPECT_NEAR(next.fraction, 0.0, 1e-12);
}
