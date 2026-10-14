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
 * @file test_metronome.cpp
 * @brief Tests where and when Metronome adds its clicks.
 * @author Mustafa Garip
 */

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/nodes/capture/metronome.hpp>
#include <SushiDSP/nodes/capture/musical_clock.hpp>
#include <SushiDSP/nodes/capture/transport.hpp>
#include <SushiDSP/foundation/math/fft.hpp>

namespace
{
    /// @brief How far apart two non-zero samples must be to belong to different clicks. A burst
    /// is 1440 frames at 48 kHz and crosses zero inside itself, so a run of zeroes alone does
    /// not separate two clicks; 2000 frames is longer than a burst and shorter than a beat at
    /// every tempo these cases use.
    constexpr std::int64_t K_ONSET_GAP_FRAMES = 2000;

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

            for (std::uint32_t frame = 0; frame < count; ++frame)
            {
                rendered[at + frame] = block[frame];
            }

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

    /// @brief Returns the bin holding the largest magnitude of a 4096-point spectrum of one
    /// burst.
    std::uint32_t peak_bin(const std::vector<float>& rendered, std::size_t offset)
    {
        const SushiDSP::Fft fft(4096u);
        std::vector<float> window(4096u, 0.0f);
        std::vector<float> real(4096u, 0.0f);
        std::vector<float> imaginary(4096u, 0.0f);

        for (std::size_t index = 0; index < 1440u && offset + index < rendered.size(); ++index)
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
} // namespace

TEST(Metronome, MetronomeAddsNothingWhenDisabled)
{
    SushiDSP::MusicalClock clock;
    SushiDSP::Metronome metronome(48000.0);
    SushiDSP::Transport transport;
    transport.play();

    const std::vector<float> rendered = render_metronome(metronome, transport, clock, 4096u, 256u);

    for (float sample : rendered)
    {
        EXPECT_EQ(sample, 0.0f);
    }
}

TEST(Metronome, MetronomeAddsNothingWhenTheTransportIsStopped)
{
    SushiDSP::MusicalClock clock;
    SushiDSP::Metronome metronome(48000.0);
    metronome.set_enabled(true);
    metronome.set_level(1.0f);

    SushiDSP::Transport transport;

    const std::vector<float> rendered = render_metronome(metronome, transport, clock, 4096u, 256u);

    for (float sample : rendered)
    {
        EXPECT_EQ(sample, 0.0f);
    }
}

TEST(Metronome, MetronomeStartsAClickOnEveryBeatFrame)
{
    SushiDSP::MusicalClock clock;
    clock.set_tempo(137.4);

    SushiDSP::Metronome metronome(48000.0);
    metronome.set_enabled(true);
    metronome.set_level(1.0f);

    SushiDSP::Transport transport;
    transport.play();

    const std::uint32_t five_minutes = 48000u * 300u;
    const std::vector<float> rendered =
        render_metronome(metronome, transport, clock, five_minutes, 256u);
    const std::vector<std::uint64_t> onsets = onset_frames(rendered, 0u);

    ASSERT_EQ(onsets.size(), 688u); // 300 s at 137.4 BPM, counting the beat at frame 0

    for (std::size_t beat = 0; beat < onsets.size(); ++beat)
    {
        EXPECT_EQ(onsets[beat],
                  clock.frame_of(beat / 4u + 1u, static_cast<std::uint32_t>(beat % 4u) + 1u))
            << "beat " << beat;
    }
}

TEST(Metronome, MetronomeKeepsTheGridAcrossASeek)
{
    SushiDSP::MusicalClock clock;
    clock.set_tempo(137.4);

    SushiDSP::Metronome metronome(48000.0);
    metronome.set_enabled(true);
    metronome.set_level(1.0f);

    SushiDSP::Transport transport;
    transport.play();

    render_metronome(metronome, transport, clock, 48000u, 256u);

    const std::uint64_t landing = 1234567u;
    transport.seek(landing);

    const std::vector<float> rendered =
        render_metronome(metronome, transport, clock, 48000u * 10u, 256u);
    const std::vector<std::uint64_t> onsets = onset_frames(rendered, landing);

    std::vector<std::uint64_t> expected;

    for (std::uint64_t beat = 0;; ++beat)
    {
        const std::uint64_t frame =
            clock.frame_of(beat / 4u + 1u, static_cast<std::uint32_t>(beat % 4u) + 1u);

        if (frame >= landing + 48000u * 10u)
        {
            break;
        }

        if (frame >= landing)
        {
            expected.push_back(frame);
        }
    }

    ASSERT_EQ(expected.size(), 23u); // 10 s at 137.4 BPM from the first beat past the landing
    EXPECT_EQ(onsets, expected);
}

TEST(Metronome, MetronomeAccentsTheDownbeat)
{
    SushiDSP::MusicalClock clock;
    SushiDSP::Metronome metronome(48000.0);
    metronome.set_enabled(true);
    metronome.set_level(1.0f);

    SushiDSP::Transport transport;
    transport.play();

    // 120 BPM in 4/4 at 48 kHz puts beat 1 of bar 1 at frame 0 and beat 2 at frame 24000.
    const std::vector<float> rendered = render_metronome(metronome, transport, clock, 48000u, 256u);

    EXPECT_EQ(peak_bin(rendered, 0u), 150u);    // 1757.8 Hz, the 1760 Hz downbeat
    EXPECT_EQ(peak_bin(rendered, 24000u), 75u); // 878.9 Hz, the 880 Hz beat
}

TEST(Metronome, MetronomeCarriesABurstAcrossABlockBoundary)
{
    SushiDSP::MusicalClock clock;

    SushiDSP::Metronome split(48000.0);
    split.set_enabled(true);
    split.set_level(1.0f);
    SushiDSP::Transport split_transport;
    split_transport.play();

    SushiDSP::Metronome whole(48000.0);
    whole.set_enabled(true);
    whole.set_level(1.0f);
    SushiDSP::Transport whole_transport;
    whole_transport.play();

    const std::vector<float> in_six = render_metronome(split, split_transport, clock, 1536u, 256u);
    const std::vector<float> in_one = render_metronome(whole, whole_transport, clock, 1536u, 1536u);

    for (std::size_t frame = 0; frame < in_six.size(); ++frame)
    {
        ASSERT_EQ(in_six[frame], in_one[frame]) << "frame " << frame;
    }

    // 30 ms at 48 kHz is 1440 frames, so the burst's last sample is frame 1439.
    EXPECT_NE(in_six[1439], 0.0f);

    for (std::size_t frame = 1440u; frame < in_six.size(); ++frame)
    {
        EXPECT_EQ(in_six[frame], 0.0f) << "frame " << frame;
    }

    double largest_step = 0.0;

    for (std::size_t frame = 1; frame < in_six.size(); ++frame)
    {
        largest_step = std::max(largest_step, std::fabs(static_cast<double>(in_six[frame]) -
                                                        static_cast<double>(in_six[frame - 1])));
    }

    EXPECT_NEAR(largest_step, 0.199471109, 1e-6); // inside the attack, across frame 54
    EXPECT_LT(std::fabs(static_cast<double>(in_six[256]) - static_cast<double>(in_six[255])),
              largest_step);
}
