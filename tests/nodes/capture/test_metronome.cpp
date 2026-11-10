/**************************************************************************/
/* test_metronome.cpp                                                     */
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
 * @file test_metronome.cpp
 * @brief Tests where and when Metronome adds its clicks.
 * @author Mustafa Garip
 */

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/allocation_trap/allocation_trap.hpp>
#include <SushiDSP/nodes/capture/metronome.hpp>
#include <SushiDSP/nodes/capture/musical_clock.hpp>
#include <SushiDSP/nodes/capture/transport.hpp>
#include <SushiDSP/foundation/math/fft.hpp>

#include "support/tolerance.hpp"

namespace
{
    /// @brief How far apart two non-zero samples must be to belong to different clicks. A burst
    /// is 1440 frames at 48 kHz and crosses zero inside itself, so a run of zeroes alone does
    /// not separate two clicks; 2000 frames is longer than a burst and shorter than a beat at
    /// every tempo these cases use.
    constexpr std::int64_t K_ONSET_GAP_FRAMES = 2000;

    constexpr double K_SAMPLE_RATE = 48000.0;
    constexpr double K_PI = 3.141592653589793;
    constexpr std::size_t K_BURST_FRAMES = 1440u;

    /// @brief Builds a metronome that is on at a level.
    void switch_on(SushiDSP::Metronome& metronome, float level)
    {
        metronome.set_enabled(true);
        metronome.set_level(level);
    }

    /// @brief Renders a metronome into one contiguous buffer, block by block, advancing the
    /// transport between blocks the way the host does.
    std::vector<float> render_metronome(SushiDSP::Metronome& metronome,
                                        SushiDSP::Transport& transport,
                                        const SushiDSP::MusicalClock& clock,
                                        std::uint32_t total_frames, std::uint32_t block_frames)
    {
        std::vector<float> rendered(total_frames, 0.0f);

        for (std::uint32_t at = 0; at < total_frames; at += block_frames)
        {
            const std::uint32_t count = std::min(block_frames, total_frames - at);
            std::vector<float> block(count, 0.0f);
            float* outputs[1] = {block.data()};

            metronome.process(outputs, 1u, count, transport, clock);

            std::copy(block.begin(), block.end(), rendered.begin() + at);

            transport.advance(count);
        }

        return rendered;
    }

    /// @brief Returns the frame each click starts at, reading a rendered buffer whose silent
    /// stretches are exactly zero.
    std::vector<std::uint64_t> onset_frames(const std::vector<float>& rendered,
                                            std::uint64_t first_frame)
    {
        std::vector<std::uint64_t> onsets;
        std::int64_t last_non_zero = -K_ONSET_GAP_FRAMES * 2;

        for (std::size_t frame = 0; frame < rendered.size(); ++frame)
        {
            if (rendered[frame] == 0.0f)
            {
                continue;
            }

            const auto absolute = static_cast<std::int64_t>(frame);

            if (absolute - last_non_zero > K_ONSET_GAP_FRAMES)
            {
                onsets.push_back(first_frame + static_cast<std::uint64_t>(frame));
            }

            last_non_zero = absolute;
        }

        return onsets;
    }

    /// @brief Returns the frames of the beats that start in [from_frame, to_frame), counting
    /// beats from the grid's frame zero.
    std::vector<std::uint64_t> expected_beat_frames(const SushiDSP::MusicalClock& clock,
                                                    std::uint64_t from_frame,
                                                    std::uint64_t to_frame)
    {
        std::vector<std::uint64_t> frames;

        for (std::uint64_t beat = 0;; ++beat)
        {
            const std::uint64_t frame =
                clock.frame_of(beat / 4u + 1u, static_cast<std::uint32_t>(beat % 4u) + 1u);

            if (frame >= to_frame)
            {
                return frames;
            }

            if (frame >= from_frame)
            {
                frames.push_back(frame);
            }
        }
    }

    /// @brief Returns the bin holding the largest magnitude of a 4096-point spectrum of one
    /// burst.
    std::uint32_t peak_bin(const std::vector<float>& rendered, std::size_t offset)
    {
        const SushiDSP::Fft fft(4096u);
        std::vector<float> window(4096u, 0.0f);
        std::vector<float> real(4096u, 0.0f);
        std::vector<float> imaginary(4096u, 0.0f);

        for (std::size_t index = 0; index < K_BURST_FRAMES && offset + index < rendered.size();
             ++index)
        {
            window[index] = rendered[offset + index];
        }

        fft.forward(window.data(), real.data(), imaginary.data());

        std::uint32_t best = 0;
        double largest = 0.0;

        for (std::uint32_t bin = 1; bin < 2048u; ++bin)
        {
            const double magnitude = std::hypot(real[bin], imaginary[bin]);

            if (magnitude > largest)
            {
                largest = magnitude;
                best = bin;
            }
        }

        return best;
    }

    /// @brief Builds one click burst from the header's description: a sine at the frequency under
    /// a raised-cosine attack of K_ATTACK_SECONDS and an exponential decay of K_DECAY_SECONDS,
    /// sampled from one frame in.
    std::vector<double> closed_form_burst(double frequency_hertz)
    {
        const double attack_frames = SushiDSP::Metronome::K_ATTACK_SECONDS * K_SAMPLE_RATE;
        std::vector<double> burst(K_BURST_FRAMES, 0.0);

        for (std::size_t index = 0; index < burst.size(); ++index)
        {
            const double frame = static_cast<double>(index + 1);
            const double seconds = frame / K_SAMPLE_RATE;
            const double attack =
                frame < attack_frames ? 0.5 * (1.0 - std::cos(K_PI * frame / attack_frames)) : 1.0;
            const double decay = std::exp(-seconds / SushiDSP::Metronome::K_DECAY_SECONDS);
            burst[index] = std::sin(2.0 * K_PI * frequency_hertz * seconds) * attack * decay;
        }

        return burst;
    }

    /// @brief Runs blocks through a stereo output with the trap armed, clearing the output before
    /// each block the way a host does.
    void process_stereo_blocks(SushiDSP::Metronome& metronome, SushiDSP::Transport& transport,
                               const SushiDSP::MusicalClock& clock, std::vector<float>& left,
                               std::vector<float>& right, int blocks,
                               SushiDSP::test::AllocationTrap& trap)
    {
        float* outputs[2] = {left.data(), right.data()};
        const auto frames = static_cast<std::uint32_t>(left.size());

        trap.arm();

        for (int block = 0; block < blocks; ++block)
        {
            std::fill(left.begin(), left.end(), 0.0f);
            std::fill(right.begin(), right.end(), 0.0f);
            metronome.process(outputs, 2u, frames, transport, clock);
            transport.advance(frames);
        }

        trap.disarm();
    }

    void expect_silent(const std::vector<float>& rendered)
    {
        for (std::size_t frame = 0; frame < rendered.size(); ++frame)
        {
            EXPECT_EQ(rendered[frame], 0.0f) << "frame " << frame;
        }
    }

    void expect_finite(const std::vector<float>& samples)
    {
        for (std::size_t frame = 0; frame < samples.size(); ++frame)
        {
            EXPECT_TRUE(std::isfinite(samples[frame])) << "frame " << frame;
        }
    }

    /// @brief Expects two buffers to hold the same bits.
    void expect_identical(const std::vector<float>& actual, const std::vector<float>& expected)
    {
        ASSERT_EQ(actual.size(), expected.size());

        for (std::size_t frame = 0; frame < expected.size(); ++frame)
        {
            ASSERT_EQ(actual[frame], expected[frame]) << "frame " << frame;
        }
    }

    /// @brief Expects the rendered burst to follow the closed form, each sample within the one
    /// rounding that stores a double as a float and the one that scales it by the level.
    void expect_burst_matches(const std::vector<float>& rendered, const std::vector<double>& burst,
                              double level)
    {
        for (std::size_t frame = 0; frame < burst.size(); ++frame)
        {
            EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_relative_error<float>(
                rendered[frame], level * burst[frame], 2)))
                << "frame " << frame;
        }
    }

    /// @brief Renders seconds of a 137.4 BPM click and expects one onset on each beat frame.
    void expect_a_click_on_every_beat(std::uint32_t seconds)
    {
        SushiDSP::MusicalClock clock;
        clock.set_tempo(137.4);
        SushiDSP::Metronome metronome(K_SAMPLE_RATE);
        switch_on(metronome, 1.0f);
        SushiDSP::Transport transport;
        transport.play();

        const std::uint32_t total_frames = 48000u * seconds;
        const std::vector<float> rendered =
            render_metronome(metronome, transport, clock, total_frames, 256u);

        EXPECT_EQ(onset_frames(rendered, 0u), expected_beat_frames(clock, 0u, total_frames));
    }
} // namespace

TEST(Small_Metronome, MetronomeAddsNothingWhenDisabled)
{
    SushiDSP::MusicalClock clock;
    SushiDSP::Metronome metronome(K_SAMPLE_RATE);
    SushiDSP::Transport transport;
    transport.play();

    const std::vector<float> rendered = render_metronome(metronome, transport, clock, 4096u, 256u);

    expect_silent(rendered);
}

TEST(Small_Metronome, MetronomeAddsNothingWhenTheTransportIsStopped)
{
    SushiDSP::MusicalClock clock;
    SushiDSP::Metronome metronome(K_SAMPLE_RATE);
    switch_on(metronome, 1.0f);
    SushiDSP::Transport transport;

    const std::vector<float> rendered = render_metronome(metronome, transport, clock, 4096u, 256u);

    expect_silent(rendered);
}

TEST(Small_Metronome, MetronomeStartsAClickOnEveryBeatFrame)
{
    // Ten seconds at 137.4 BPM hold the beats at frames 20960.5 apart: beats 0 to 22.
    expect_a_click_on_every_beat(10u);
}

TEST(Large_Metronome, MetronomeStartsAClickOnEveryBeatFrameOverFiveMinutes)
{
    // Five minutes are 14.4 million frames, too long for a small test. The grid must not drift.
    expect_a_click_on_every_beat(300u);
}

TEST(Small_Metronome, MetronomeKeepsTheGridAcrossASeek)
{
    SushiDSP::MusicalClock clock;
    clock.set_tempo(137.4);
    SushiDSP::Metronome metronome(K_SAMPLE_RATE);
    switch_on(metronome, 1.0f);
    SushiDSP::Transport transport;
    transport.play();
    render_metronome(metronome, transport, clock, 48000u, 256u);
    const std::uint64_t landing = 1234567u;
    transport.seek(landing);

    const std::vector<float> rendered =
        render_metronome(metronome, transport, clock, 48000u * 10u, 256u);

    const std::vector<std::uint64_t> expected =
        expected_beat_frames(clock, landing, landing + 48000u * 10u);
    ASSERT_EQ(expected.size(), 23u); // 10 s at 137.4 BPM from the first beat past the landing
    EXPECT_EQ(onset_frames(rendered, landing), expected);
}

TEST(Small_Metronome, MetronomeAccentsTheDownbeat)
{
    SushiDSP::MusicalClock clock;
    SushiDSP::Metronome metronome(K_SAMPLE_RATE);
    switch_on(metronome, 1.0f);
    SushiDSP::Transport transport;
    transport.play();

    // 120 BPM in 4/4 at 48 kHz puts beat 1 of bar 1 at frame 0 and beat 2 at frame 24000.
    const std::vector<float> rendered = render_metronome(metronome, transport, clock, 48000u, 256u);

    EXPECT_EQ(peak_bin(rendered, 0u), 150u);    // 1757.8 Hz, the 1760 Hz downbeat
    EXPECT_EQ(peak_bin(rendered, 24000u), 75u); // 878.9 Hz, the 880 Hz beat
}

TEST(Small_Metronome, MetronomeRendersTheDownbeatBurstOfItsClosedForm)
{
    SushiDSP::MusicalClock clock;
    SushiDSP::Metronome metronome(K_SAMPLE_RATE);
    switch_on(metronome, 1.0f);
    SushiDSP::Transport transport;
    transport.play();

    const std::vector<float> rendered = render_metronome(metronome, transport, clock, 1536u, 1536u);

    expect_burst_matches(rendered, closed_form_burst(SushiDSP::Metronome::K_DOWNBEAT_HERTZ), 1.0);
}

TEST(Small_Metronome, MetronomeScalesTheClickByItsLevel)
{
    SushiDSP::MusicalClock clock;
    SushiDSP::Metronome full(K_SAMPLE_RATE);
    switch_on(full, 1.0f);
    SushiDSP::Metronome half(K_SAMPLE_RATE);
    switch_on(half, 0.5f);
    SushiDSP::Transport full_transport;
    full_transport.play();
    SushiDSP::Transport half_transport;
    half_transport.play();

    const std::vector<float> loud = render_metronome(full, full_transport, clock, 1536u, 1536u);
    std::vector<float> halved = render_metronome(half, half_transport, clock, 1536u, 1536u);

    // Halving is exact in binary floating point, so the doubled render equals the loud one.
    std::transform(halved.begin(), halved.end(), halved.begin(), [](float v) { return v * 2.0f; });
    expect_identical(halved, loud);
}

TEST(Small_Metronome, MetronomeCarriesABurstAcrossABlockBoundary)
{
    SushiDSP::MusicalClock clock;
    SushiDSP::Metronome split(K_SAMPLE_RATE);
    switch_on(split, 1.0f);
    SushiDSP::Transport split_transport;
    split_transport.play();
    SushiDSP::Metronome whole(K_SAMPLE_RATE);
    switch_on(whole, 1.0f);
    SushiDSP::Transport whole_transport;
    whole_transport.play();

    const std::vector<float> in_six = render_metronome(split, split_transport, clock, 1536u, 256u);
    const std::vector<float> in_one = render_metronome(whole, whole_transport, clock, 1536u, 1536u);

    expect_identical(in_six, in_one);
}

TEST(Small_Metronome, MetronomeBurstEndsThirtyMillisecondsAfterItsBeat)
{
    SushiDSP::MusicalClock clock;
    SushiDSP::Metronome metronome(K_SAMPLE_RATE);
    switch_on(metronome, 1.0f);
    SushiDSP::Transport transport;
    transport.play();

    const std::vector<float> rendered = render_metronome(metronome, transport, clock, 1536u, 256u);

    // 30 ms at 48 kHz is 1440 frames, so the burst's last sample is frame 1439.
    EXPECT_NE(rendered[K_BURST_FRAMES - 1u], 0.0f);
    expect_silent(std::vector<float>(rendered.begin() + K_BURST_FRAMES, rendered.end()));
}

TEST(Medium_Metronome, ProcessPerformsNoHeapAllocation)
{
    // The bursts are built before the trap opens. See tests/nodes/capture/README.md.
    SushiDSP::MusicalClock clock;
    clock.set_tempo(137.4);
    SushiDSP::Metronome metronome(K_SAMPLE_RATE);
    switch_on(metronome, 0.5f);
    SushiDSP::Transport transport;
    transport.play();
    std::vector<float> left(256, 0.0f);
    std::vector<float> right(256, 0.0f);
    SushiDSP::test::AllocationTrap trap;

    process_stereo_blocks(metronome, transport, clock, left, right, 400, trap);

    EXPECT_EQ(trap.count(), 0);
    expect_finite(left);
    expect_finite(right);
}
