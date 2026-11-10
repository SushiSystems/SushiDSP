/**************************************************************************/
/* test_musical_clock.cpp                                                 */
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
 * @file test_musical_clock.cpp
 * @brief Tests how MusicalClock maps frames to bars and beats and back.
 * @author Mustafa Garip
 */

#include <cmath>
#include <cstdint>
#include <string>

#include <gtest/gtest.h>

#include <SushiDSP/nodes/capture/musical_clock.hpp>

#include "support/tolerance.hpp"

namespace
{
    /// @brief Names a clock setting for a round-trip case.
    struct RoundTripRow
    {
        const char* name;
        double sample_rate;
        double tempo;
    };

    /// @brief Names a published-step case: the settings and the 32.32 frames per beat they give.
    struct StepRow
    {
        const char* name;
        double sample_rate;
        double tempo;
        std::uint32_t beats_per_bar;
        std::uint32_t beat_unit;
        std::uint64_t step_q32;
    };

    /// @brief Names a setter call that the clock must refuse.
    struct RefusedRow
    {
        const char* name;
        void (*apply)(SushiDSP::MusicalClock&);
    };

    /// @brief Names a bar of the 137.4 BPM grid far down the timeline.
    struct FarBarRow
    {
        const char* name;
        std::uint64_t bar;
    };

    /// @brief Names a frame inside the first beat of the default grid.
    struct FirstBeatRow
    {
        const char* name;
        std::uint64_t frame;
        std::uint32_t beat;
        double fraction;
    };

    SushiDSP::MusicalClock& configure(SushiDSP::MusicalClock& clock, double sample_rate,
                                      double tempo, std::uint32_t beats_per_bar,
                                      std::uint32_t beat_unit)
    {
        clock.set_sample_rate(sample_rate);
        clock.set_tempo(tempo);
        clock.set_meter(beats_per_bar, beat_unit);
        return clock;
    }

    /// @brief Returns the first frame of beat index n at 137.4 BPM and 48 kHz by integer
    /// arithmetic: 137.4 is 1374/10, so a beat lasts 48000 * 60 * 10 / 1374 frames.
    std::uint64_t exact_frame_of_beat(std::uint64_t beat)
    {
        return beat * 28800000ull / 1374ull;
    }

    /// @brief Expects every beat start in the range to equal the exact frame, rounded down.
    void expect_beats_on_the_exact_grid(const SushiDSP::MusicalClock& clock, std::uint64_t beats)
    {
        for (std::uint64_t beat = 0; beat < beats; ++beat)
        {
            EXPECT_EQ(clock.frame_of(beat / 4u + 1u, static_cast<std::uint32_t>(beat % 4u) + 1u),
                      exact_frame_of_beat(beat))
                << "beat " << beat;
        }
    }

    /// @brief Expects each frame to read back to a beat whose start is at or before it and less
    /// than one beat step behind it.
    void expect_frames_read_back_to_their_beat(const SushiDSP::MusicalClock& clock)
    {
        const std::uint64_t step = clock.beat_step_q32();

        for (std::uint64_t frame = 0; frame < 4000000u; frame += 7919u)
        {
            const SushiDSP::BarBeat at = clock.position_of(frame);
            const std::uint64_t beat_start = clock.frame_of(at.bar, at.beat);
            EXPECT_LE(beat_start, frame);
            EXPECT_LT(frame - beat_start, (step >> 32) + 1u);
        }
    }

    /// @brief Expects two clocks to publish the same step and report the same settings.
    void expect_same_settings(const SushiDSP::MusicalClock& actual,
                              const SushiDSP::MusicalClock& expected)
    {
        EXPECT_EQ(actual.beat_step_q32(), expected.beat_step_q32());
        EXPECT_EQ(actual.beats_per_minute(), expected.beats_per_minute());
        EXPECT_EQ(actual.beats_per_bar(), expected.beats_per_bar());
        EXPECT_EQ(actual.beat_unit(), expected.beat_unit());
        EXPECT_EQ(actual.sample_rate(), expected.sample_rate());
    }

    /// @brief Expects a fraction within the rounding of the few double operations that make it.
    void expect_fraction(double actual, double expected)
    {
        EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_relative_error<double>(actual, expected, 4)));
    }
} // namespace

TEST(Small_MusicalClock, MusicalClockPlacesBarOneHundredAtTheExactFrame)
{
    SushiDSP::MusicalClock clock;
    configure(clock, 48000.0, 120.0, 4, 4);

    EXPECT_EQ(clock.frame_of(100, 1), 9504000u);
    const SushiDSP::BarBeat at = clock.position_of(9504000u);
    EXPECT_EQ(at.bar, 100u);
    EXPECT_EQ(at.beat, 1u);
    expect_fraction(at.fraction, 0.0);
}

class Small_MusicalClockRoundTrip : public ::testing::TestWithParam<RoundTripRow>
{
};

TEST_P(Small_MusicalClockRoundTrip, MusicalClockRoundTripsEveryFrameToItsOwnBeat)
{
    SushiDSP::MusicalClock clock;
    configure(clock, GetParam().sample_rate, GetParam().tempo, 4, 4);

    expect_frames_read_back_to_their_beat(clock);
}

INSTANTIATE_TEST_SUITE_P(
    Rows, Small_MusicalClockRoundTrip,
    ::testing::Values(RoundTripRow{"Rate44100Tempo120", 44100.0, 120.0},
                      RoundTripRow{"Rate44100Tempo1374", 44100.0, 137.4},
                      RoundTripRow{"Rate48000Tempo120", 48000.0, 120.0},
                      RoundTripRow{"Rate48000Tempo1374", 48000.0, 137.4},
                      RoundTripRow{"Rate96000Tempo120", 96000.0, 120.0},
                      RoundTripRow{"Rate96000Tempo1374", 96000.0, 137.4},
                      RoundTripRow{"Rate192000Tempo120", 192000.0, 120.0},
                      RoundTripRow{"Rate192000Tempo1374", 192000.0, 137.4}),
    [](const ::testing::TestParamInfo<RoundTripRow>& info) { return std::string(info.param.name); });

TEST(Small_MusicalClock, MusicalClockCountsTheBeatTheMeterNames)
{
    SushiDSP::MusicalClock clock;
    configure(clock, 48000.0, 120.0, 6, 8);

    // A 120 BPM quarter is 24000 frames, so an eighth is 12000 and a 6/8 bar is 72000.
    EXPECT_EQ(clock.beat_step_q32() >> 32, 12000u);
    EXPECT_EQ(clock.frame_of(2, 1), 72000u);
}

TEST(Small_MusicalClock, MusicalClockDoesNotAccumulateErrorAcrossFiveMinutes)
{
    SushiDSP::MusicalClock clock;
    configure(clock, 48000.0, 137.4, 4, 4);

    // 300 s at 137.4 BPM hold 687 beats. Each start is the exact frame, rounded down.
    expect_beats_on_the_exact_grid(clock, 687u);
}

class Small_MusicalClockStep : public ::testing::TestWithParam<StepRow>
{
};

TEST_P(Small_MusicalClockStep, MusicalClockPublishesTheStepTheTempoAndMeterName)
{
    SushiDSP::MusicalClock clock;
    configure(clock, GetParam().sample_rate, GetParam().tempo, GetParam().beats_per_bar,
              GetParam().beat_unit);

    EXPECT_EQ(clock.beat_step_q32(), GetParam().step_q32);
}

/**
 * 48000 * 60 / 120 is 24000 frames a quarter, and 24000 * 2^32 is exact in a double. 44100 * 60 / 90
 * is 29400, so the fractional half of the word is zero. 48000 * 60 / 137.4 is 20960.698689956 frames
 * a quarter, rounded to the nearest 2^-32. An eighth-note beat is exactly half the quarter. See
 * tests/nodes/capture/README.md.
 */
INSTANTIATE_TEST_SUITE_P(
    Rows, Small_MusicalClockStep,
    ::testing::Values(StepRow{"Quarter120At48000", 48000.0, 120.0, 4, 4, 24000ull << 32},
                      StepRow{"Quarter90At44100", 44100.0, 90.0, 4, 4, 29400ull << 32},
                      StepRow{"Quarter1374At48000", 48000.0, 137.4, 4, 4, 90025515374672ull},
                      StepRow{"Eighth1374At48000", 48000.0, 137.4, 7, 8, 45012757687336ull}),
    [](const ::testing::TestParamInfo<StepRow>& info) { return std::string(info.param.name); });

TEST(Small_MusicalClock, MusicalClockPlacesAnAwkwardMeterAtAHandComputedFrame)
{
    SushiDSP::MusicalClock clock;
    configure(clock, 48000.0, 137.4, 7, 8);

    // Bar 41 beat 3 is absolute eighth 282, and (282 * 45012757687336) >> 32 is 2955458.
    EXPECT_EQ(clock.frame_of(41, 3), 2955458u);
    const SushiDSP::BarBeat at = clock.position_of(2955458u + 5000u);
    EXPECT_EQ(at.bar, 41u);
    EXPECT_EQ(at.beat, 3u);

    /* The eighth truly begins at 282 * step / 2^32 frames. The frame sits 4999.484 into a 10480.349
       frame beat. See tests/nodes/capture/README.md. */
    const long double frames_per_beat = static_cast<long double>(clock.beat_step_q32()) / 4294967296.0L;
    const long double true_start = 282.0L * frames_per_beat;
    const long double expected = (2955458.0L + 5000.0L - true_start) / frames_per_beat;
    expect_fraction(at.fraction, static_cast<double>(expected));
}

class Small_MusicalClockFarBar : public ::testing::TestWithParam<FarBarRow>
{
};

TEST_P(Small_MusicalClockFarBar, MusicalClockKeepsBarStartsWithinOneSampleFarDownTheTimeline)
{
    SushiDSP::MusicalClock clock;
    configure(clock, 48000.0, 137.4, 4, 4);

    /* Every start derives from the absolute beat index. See tests/nodes/capture/README.md. The
       start is the ideal frame rounded down, so it never lies above the ideal and less than one
       sample below it. */
    EXPECT_EQ(clock.frame_of(GetParam().bar, 1), exact_frame_of_beat((GetParam().bar - 1u) * 4u));
}

INSTANTIATE_TEST_SUITE_P(
    Rows, Small_MusicalClockFarBar,
    ::testing::Values(FarBarRow{"Bar2", 2ull}, FarBarRow{"Bar100", 100ull},
                      FarBarRow{"Bar1000", 1000ull}, FarBarRow{"Bar20000", 20000ull}),
    [](const ::testing::TestParamInfo<FarBarRow>& info) { return std::string(info.param.name); });

class Small_MusicalClockRefusal : public ::testing::TestWithParam<RefusedRow>
{
};

TEST_P(Small_MusicalClockRefusal, MusicalClockIgnoresATempoMeterOrRateThatCannotBeUsed)
{
    SushiDSP::MusicalClock clock;
    configure(clock, 48000.0, 120.0, 4, 4);
    SushiDSP::MusicalClock untouched;
    configure(untouched, 48000.0, 120.0, 4, 4);

    GetParam().apply(clock);

    expect_same_settings(clock, untouched);
}

INSTANTIATE_TEST_SUITE_P(
    Rows, Small_MusicalClockRefusal,
    ::testing::Values(RefusedRow{"TempoZero", [](SushiDSP::MusicalClock& c) { c.set_tempo(0.0); }},
                      RefusedRow{"TempoNegative", [](SushiDSP::MusicalClock& c) { c.set_tempo(-120.0); }},
                      RefusedRow{"TempoNotANumber", [](SushiDSP::MusicalClock& c) { c.set_tempo(std::nan("")); }},
                      RefusedRow{"SampleRateZero", [](SushiDSP::MusicalClock& c) { c.set_sample_rate(0.0); }},
                      RefusedRow{"NoBeatsPerBar", [](SushiDSP::MusicalClock& c) { c.set_meter(0, 4); }},
                      RefusedRow{"NoBeatUnit", [](SushiDSP::MusicalClock& c) { c.set_meter(4, 0); }}),
    [](const ::testing::TestParamInfo<RefusedRow>& info) { return std::string(info.param.name); });

class Small_MusicalClockFirstBeat : public ::testing::TestWithParam<FirstBeatRow>
{
};

TEST_P(Small_MusicalClockFirstBeat, MusicalClockReadsAFrameInsideTheFirstBeat)
{
    SushiDSP::MusicalClock clock;
    configure(clock, 48000.0, 120.0, 4, 4);

    const SushiDSP::BarBeat at = clock.position_of(GetParam().frame);

    EXPECT_EQ(at.bar, 1u);
    EXPECT_EQ(at.beat, GetParam().beat);
    expect_fraction(at.fraction, GetParam().fraction);
}

INSTANTIATE_TEST_SUITE_P(
    Rows, Small_MusicalClockFirstBeat,
    ::testing::Values(FirstBeatRow{"HalfWayThroughBeatOne", 12000u, 1u, 0.5},
                      FirstBeatRow{"StartOfBeatTwo", 24000u, 2u, 0.0}),
    [](const ::testing::TestParamInfo<FirstBeatRow>& info) { return std::string(info.param.name); });
