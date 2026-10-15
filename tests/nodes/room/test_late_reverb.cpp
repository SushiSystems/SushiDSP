/**************************************************************************/
/* test_late_reverb.cpp                                                   */
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
 * @file test_late_reverb.cpp
 * @brief Tests for LateReverb, the 16-line Hadamard feedback delay network.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <SushiDSP/foundation/allocation_trap/allocation_trap.hpp>
#include <SushiDSP/nodes/room/fdn_design.hpp>
#include <SushiDSP/nodes/room/late_reverb.hpp>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{
    using SushiDSP::room::FdnDesign;
    using SushiDSP::room::LateReverb;

    /// @brief Counts the lines.
    constexpr std::size_t K_LINES = FdnDesign::K_LINE_COUNT;

    /// @brief Holds the sample rate every design below is written for.
    constexpr double K_SAMPLE_RATE = 48000.0;

    /// @brief Holds pi.
    constexpr double K_PI = 3.14159265358979323846;

    /// @brief Holds the longest line any design below asks for.
    constexpr std::uint32_t K_MAX_LINE_FRAMES = 8000;

    /// @brief Holds a unity section: b0 = 1, every other coefficient 0.
    constexpr FdnDesign::Section K_UNITY_SECTION = {1.0, 0.0, 0.0, 0.0, 0.0};

    /// @brief Holds a silent section: every coefficient 0.
    constexpr FdnDesign::Section K_ZERO_SECTION = {0.0, 0.0, 0.0, 0.0, 0.0};

    /// @brief Holds a cascade of two unity sections.
    constexpr FdnDesign::Cascade K_UNITY_CASCADE = {K_UNITY_SECTION, K_UNITY_SECTION};

    /// @brief Holds a cascade whose first section is silent.
    constexpr FdnDesign::Cascade K_ZERO_CASCADE = {K_ZERO_SECTION, K_UNITY_SECTION};

    /// @brief Returns a cascade of two plain gains of 0.5, a quarter overall.
    FdnDesign::Cascade half_twice()
    {
        const FdnDesign::Section half = {0.5, 0.0, 0.0, 0.0, 0.0};
        return {half, half};
    }

    /// @brief Holds sixteen primes spread over a 1 : 2 range about 100 ms at 48 kHz.
    constexpr std::array<std::uint32_t, K_LINES> K_MID_LINES = {3203, 3347, 3511, 3677, 3851, 4027,
                                                                4219, 4421, 4637, 4861, 5081, 5323,
                                                                5573, 5839, 6113, 6397};

    /// @brief Holds sixteen primes between 293 and 599 frames.
    constexpr std::array<std::uint32_t, K_LINES> K_SHORT_LINES = {
        293, 313, 331, 347, 359, 379, 397, 419, 433, 457, 479, 499, 521, 547, 571, 599};

    /// @brief Holds sixteen primes between 149 and 293 frames, every one shorter than its
    /// K_SHORT_LINES counterpart.
    constexpr std::array<std::uint32_t, K_LINES> K_SHORTER_LINES = {
        149, 157, 163, 173, 179, 191, 197, 211, 223, 227, 239, 251, 263, 271, 283, 293};

    /// @brief Returns a design with the given lines, a plain gain per line as its first section,
    /// unity everywhere else, the left ear reading A alone and the right ear reading B alone.
    FdnDesign make_design(const std::array<std::uint32_t, K_LINES>& lines,
                          const std::array<double, K_LINES>& line_gains)
    {
        FdnDesign design{};
        design.line_frames = lines;
        for (std::size_t line = 0; line < K_LINES; ++line)
        {
            design.line_section_a[line] = {line_gains[line], 0.0, 0.0, 0.0, 0.0};
            design.line_section_b[line] = K_UNITY_SECTION;
        }
        design.tone_correction = K_UNITY_CASCADE;
        design.coherence_a = K_ZERO_CASCADE;
        design.coherence_b = K_UNITY_CASCADE;
        design.input_gain = 1.0f;
        design.delay_to_crossover_frames = 0.0f;
        return design;
    }

    /// @brief Returns the gains that make every line lose 60 dB in rt60 seconds of
    /// recirculation: 10^(-3 * tau_i / rt60).
    std::array<double, K_LINES> decay_gains(const std::array<std::uint32_t, K_LINES>& lines,
                                            double rt60_seconds)
    {
        std::array<double, K_LINES> gains{};
        for (std::size_t line = 0; line < K_LINES; ++line)
        {
            const double tau = static_cast<double>(lines[line]) / K_SAMPLE_RATE;
            gains[line] = std::pow(10.0, -3.0 * tau / rt60_seconds);
        }
        return gains;
    }

    /// @brief Returns sixteen copies of one gain.
    std::array<double, K_LINES> uniform_gains(double gain)
    {
        std::array<double, K_LINES> gains{};
        gains.fill(gain);
        return gains;
    }

    /// @brief Holds both ears' output over one rendering.
    struct Rendering
    {
            /// @brief Stores the left ear.
            std::vector<float> left;

            /// @brief Stores the right ear.
            std::vector<float> right;
    };

    /// @brief Renders one input through a prepared network in blocks of 256 frames.
    Rendering render(LateReverb& reverb, const FdnDesign& design, const std::vector<float>& input)
    {
        constexpr std::uint32_t K_BLOCK = 256;
        Rendering out;
        out.left.assign(input.size(), 0.0f);
        out.right.assign(input.size(), 0.0f);
        for (std::size_t start = 0; start < input.size(); start += K_BLOCK)
        {
            const auto frames =
                static_cast<std::uint32_t>(std::min<std::size_t>(K_BLOCK, input.size() - start));
            reverb.process(design, input.data() + start, out.left.data() + start,
                           out.right.data() + start, frames);
        }
        return out;
    }

    /// @brief Returns a unit impulse at frame 0 followed by silence.
    std::vector<float> impulse(std::size_t frames)
    {
        std::vector<float> signal(frames, 0.0f);
        signal[0] = 1.0f;
        return signal;
    }

    /// @brief Returns the sum of squares of a signal.
    double energy(const std::vector<float>& signal)
    {
        double sum = 0.0;
        for (const float sample : signal)
        {
            sum += static_cast<double>(sample) * static_cast<double>(sample);
        }
        return sum;
    }

    /// @brief Returns the time in seconds at which the Schroeder backward integral first falls
    /// 60 dB below its value at frame 0.
    double schroeder_t60(const std::vector<float>& response)
    {
        std::vector<double> tail(response.size() + 1, 0.0);
        for (std::size_t index = response.size(); index-- > 0;)
        {
            tail[index] = tail[index + 1] + static_cast<double>(response[index]) *
                                                static_cast<double>(response[index]);
        }
        const double threshold = tail[0] * 1e-6;
        for (std::size_t index = 0; index < response.size(); ++index)
        {
            if (tail[index] <= threshold)
            {
                return static_cast<double>(index) / K_SAMPLE_RATE;
            }
        }
        return static_cast<double>(response.size()) / K_SAMPLE_RATE;
    }
} // namespace

TEST(Small_LateReverb, LateReverbHadamardPreservesEnergy)
{
    // The normalised Hadamard matrix is orthogonal. See tests/nodes/room/README.md.
    std::array<double, K_LINES> values{};
    double start = 0.0;
    for (std::size_t line = 0; line < K_LINES; ++line)
    {
        values[line] =
            std::sin(0.7 * static_cast<double>(line + 1)) + 0.1 * static_cast<double>(line);
        start += values[line] * values[line];
    }
    for (int pass = 0; pass < 1000; ++pass)
    {
        LateReverb::hadamard_transform(values);
        double current = 0.0;
        for (const double value : values)
        {
            current += value * value;
        }
        EXPECT_NEAR(current / start, 1.0, 1e-5);
    }

    // The left ear reads 0 and 4 in turn without decaying. See tests/nodes/room/README.md.
    std::array<std::uint32_t, K_LINES> ones{};
    ones.fill(1u);
    const FdnDesign design = make_design(ones, uniform_gains(1.0));
    LateReverb reverb;
    reverb.prepare(K_MAX_LINE_FRAMES);
    const Rendering out = render(reverb, design, impulse(1001));
    for (std::size_t frame = 1; frame <= 1000; ++frame)
    {
        const double expected = frame % 2 == 0 ? 4.0 : 0.0;
        EXPECT_NEAR(out.left[frame], expected, 1e-5) << "frame " << frame;
    }
}

TEST(Small_LateReverb, LateReverbDecaysAtTheRateItsFiltersAsk)
{
    const FdnDesign design = make_design(K_MID_LINES, decay_gains(K_MID_LINES, 1.0));
    LateReverb reverb;
    reverb.prepare(K_MAX_LINE_FRAMES);
    const Rendering out = render(reverb, design, impulse(96000));
    EXPECT_NEAR(schroeder_t60(out.left), 1.0, 0.05);
}

TEST(Small_LateReverb, LateReverbIsSilentBeforeItsInputDelay)
{
    FdnDesign design = make_design(K_SHORT_LINES, uniform_gains(0.5));
    design.delay_to_crossover_frames = 1749.0f;
    LateReverb reverb;
    reverb.prepare(K_MAX_LINE_FRAMES);
    const Rendering out = render(reverb, design, impulse(4096));
    for (std::size_t frame = 0; frame < 1749; ++frame)
    {
        ASSERT_EQ(out.left[frame], 0.0f) << "frame " << frame;
        ASSERT_EQ(out.right[frame], 0.0f) << "frame " << frame;
    }
    // The first line is 293 frames long, so the tail arrives 293 frames after the delay.
    EXPECT_NE(out.left[1749 + 293], 0.0f);
}

TEST(Small_LateReverb, LateReverbEarsDifferButShareTheirEnergy)
{
    const FdnDesign design = make_design(K_MID_LINES, decay_gains(K_MID_LINES, 1.0));
    LateReverb reverb;
    reverb.prepare(K_MAX_LINE_FRAMES);
    const Rendering out = render(reverb, design, impulse(48000));
    EXPECT_NE(out.left, out.right);
    const double ratio_db = 10.0 * std::log10(energy(out.left) / energy(out.right));
    EXPECT_NEAR(ratio_db, 0.0, 0.5);
}

TEST(Small_LateReverb, LateReverbRunsBothSectionsOfEveryOutputFilter)
{
    // Two sections of gain 0.5 scale the output by 0.25. See tests/nodes/room/README.md.
    const FdnDesign plain = make_design(K_SHORT_LINES, uniform_gains(0.5));
    FdnDesign quarter_tone = plain;
    quarter_tone.tone_correction = half_twice();
    FdnDesign quarter_right = plain;
    quarter_right.coherence_b = half_twice();

    LateReverb reverb;
    reverb.prepare(K_MAX_LINE_FRAMES);
    const Rendering reference = render(reverb, plain, impulse(4096));
    reverb.reset();
    const Rendering tone = render(reverb, quarter_tone, impulse(4096));
    reverb.reset();
    const Rendering right = render(reverb, quarter_right, impulse(4096));
    ASSERT_GT(energy(reference.left), 0.0);
    ASSERT_GT(energy(reference.right), 0.0);
    for (std::size_t frame = 0; frame < 4096; ++frame)
    {
        ASSERT_NEAR(tone.left[frame], 0.25f * reference.left[frame], 1e-6f) << "frame " << frame;
        ASSERT_NEAR(tone.right[frame], 0.25f * reference.right[frame], 1e-6f) << "frame " << frame;
        ASSERT_EQ(right.left[frame], reference.left[frame]) << "frame " << frame;
        ASSERT_NEAR(right.right[frame], 0.25f * reference.right[frame], 1e-6f) << "frame " << frame;
    }
}

TEST(Small_LateReverb, LateReverbResetSilencesTheLines)
{
    const FdnDesign design = make_design(K_SHORT_LINES, uniform_gains(0.9));
    LateReverb reverb;
    reverb.prepare(K_MAX_LINE_FRAMES);
    render(reverb, design, impulse(4096));
    reverb.reset();
    const Rendering out = render(reverb, design, std::vector<float>(4096, 0.0f));
    for (std::size_t frame = 0; frame < 4096; ++frame)
    {
        ASSERT_EQ(out.left[frame], 0.0f) << "frame " << frame;
        ASSERT_EQ(out.right[frame], 0.0f) << "frame " << frame;
    }
}

TEST(Small_LateReverb, LateReverbSurvivesALineLengthChangeWithoutAClick)
{
    // A 50 Hz sine steps at most 0.0065 of peak per sample. See tests/nodes/room/README.md.
    constexpr std::size_t K_BLOCK = 1024;
    std::vector<float> sine(2 * K_BLOCK);
    for (std::size_t frame = 0; frame < sine.size(); ++frame)
    {
        sine[frame] = static_cast<float>(
            std::sin(2.0 * K_PI * 50.0 * static_cast<double>(frame) / K_SAMPLE_RATE));
    }
    const FdnDesign before = make_design(K_SHORT_LINES, uniform_gains(0.5));
    const FdnDesign after = make_design(K_SHORTER_LINES, uniform_gains(0.5));
    LateReverb reverb;
    reverb.prepare(K_MAX_LINE_FRAMES);
    std::vector<float> left(sine.size(), 0.0f);
    std::vector<float> right(sine.size(), 0.0f);
    reverb.process(before, sine.data(), left.data(), right.data(), K_BLOCK);
    reverb.process(after, sine.data() + K_BLOCK, left.data() + K_BLOCK, right.data() + K_BLOCK,
                   K_BLOCK);

    float peak = 0.0f;
    for (std::size_t frame = K_BLOCK; frame < sine.size(); ++frame)
    {
        peak = std::max(peak, std::fabs(left[frame]));
    }
    ASSERT_GT(peak, 0.0f);
    for (std::size_t frame = K_BLOCK; frame < sine.size(); ++frame)
    {
        const float step = std::fabs(left[frame] - left[frame - 1]);
        EXPECT_LE(step, 0.05f * peak) << "frame " << frame;
    }
}

namespace
{
    /// @brief Fills a block with a 220 Hz tone at 0.3 of full scale.
    void fill_room_tone(std::vector<float>& block)
    {
        for (std::size_t i = 0; i < block.size(); ++i)
        {
            block[i] = 0.3f * std::sin(2.0f * static_cast<float>(M_PI) * 220.0f *
                                       static_cast<float>(i) / 48000.0f);
        }
    }
} // namespace

TEST(Small_LateReverb, ProcessPerformsNoHeapAllocation)
{
    // The trapped blocks include the 512-frame crossfade. See tests/nodes/room/README.md.
    SushiDSP::room::LateReverb reverb;
    reverb.prepare(25200);

    SushiDSP::room::FdnDesign design{};
    const SushiDSP::room::FdnDesign::Section unity = {1.0, 0.0, 0.0, 0.0, 0.0};
    const SushiDSP::room::FdnDesign::Section damped = {0.9, 0.0, 0.0, 0.0, 0.0};
    for (std::size_t line = 0; line < SushiDSP::room::FdnDesign::K_LINE_COUNT; ++line)
    {
        design.line_frames[line] = static_cast<std::uint32_t>(1000 + 97 * line);
        design.line_section_a[line] = damped;
        design.line_section_b[line] = unity;
    }
    design.tone_correction = {unity, unity};
    design.coherence_a = {unity, unity};
    design.coherence_b = {unity, unity};
    design.input_gain = 0.5f;
    design.delay_to_crossover_frames = 1749.0f;

    SushiDSP::room::FdnDesign moved = design;
    for (std::uint32_t& frames : moved.line_frames)
    {
        frames += 37u;
    }

    std::vector<float> input(256);
    fill_room_tone(input);
    std::vector<float> left(256, 0.0f);
    std::vector<float> right(256, 0.0f);

    SushiDSP::test::AllocationTrap trap;
    trap.arm();
    for (int block = 0; block < 8; ++block)
    {
        const SushiDSP::room::FdnDesign& current = block < 4 ? design : moved;
        reverb.process(current, input.data(), left.data(), right.data(), 256);
        reverb.process(current, input.data(), left.data(), left.data(), 256);
    }
    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
    for (std::size_t i = 0; i < left.size(); ++i)
    {
        EXPECT_TRUE(std::isfinite(left[i]));
        EXPECT_TRUE(std::isfinite(right[i]));
    }
}
