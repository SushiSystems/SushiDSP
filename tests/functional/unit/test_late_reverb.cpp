/****************************************************************/
/* test_late_reverb.cpp                                         */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

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

#include "SushiDSP/room/FdnDesign.hpp"
#include "SushiDSP/room/LateReverb.hpp"

namespace
{
    using SushiDSP::room::FdnDesign;
    using SushiDSP::room::LateReverb;

    /// @brief Counts the lines.
    constexpr std::size_t kLines = FdnDesign::kLineCount;

    /// @brief Holds the sample rate every design below is written for.
    constexpr double kSampleRate = 48000.0;

    /// @brief Holds pi.
    constexpr double kPi = 3.14159265358979323846;

    /// @brief Holds the longest line any design below asks for.
    constexpr std::uint32_t kMaxLineFrames = 8000;

    /// @brief Holds a unity section: b0 = 1, every other coefficient 0.
    constexpr FdnDesign::Section kUnitySection = { 1.0, 0.0, 0.0, 0.0, 0.0 };

    /// @brief Holds a silent section: every coefficient 0.
    constexpr FdnDesign::Section kZeroSection = { 0.0, 0.0, 0.0, 0.0, 0.0 };

    /// @brief Holds a cascade of two unity sections.
    constexpr FdnDesign::Cascade kUnityCascade = { kUnitySection, kUnitySection };

    /// @brief Holds a cascade whose first section is silent.
    constexpr FdnDesign::Cascade kZeroCascade = { kZeroSection, kUnitySection };

    /// @brief Returns a cascade of two plain gains of 0.5, a quarter overall.
    FdnDesign::Cascade half_twice()
    {
        const FdnDesign::Section half = { 0.5, 0.0, 0.0, 0.0, 0.0 };
        return { half, half };
    }

    /// @brief Holds sixteen primes spread over a 1 : 2 range about 100 ms at 48 kHz.
    constexpr std::array<std::uint32_t, kLines> kMidLines = {
        3203, 3347, 3511, 3677, 3851, 4027, 4219, 4421,
        4637, 4861, 5081, 5323, 5573, 5839, 6113, 6397
    };

    /// @brief Holds sixteen primes between 293 and 599 frames.
    constexpr std::array<std::uint32_t, kLines> kShortLines = {
        293, 313, 331, 347, 359, 379, 397, 419, 433, 457, 479, 499, 521, 547, 571, 599
    };

    /// @brief Holds sixteen primes between 149 and 293 frames, every one shorter than its
    /// kShortLines counterpart.
    constexpr std::array<std::uint32_t, kLines> kShorterLines = {
        149, 157, 163, 173, 179, 191, 197, 211, 223, 227, 239, 251, 263, 271, 283, 293
    };

    /// @brief Returns a design with the given lines, a plain gain per line as its first section,
    /// unity everywhere else, the left ear reading A alone and the right ear reading B alone.
    FdnDesign make_design(const std::array<std::uint32_t, kLines>& lines,
                          const std::array<double, kLines>& line_gains)
    {
        FdnDesign design{};
        design.line_frames = lines;
        for (std::size_t line = 0; line < kLines; ++line)
        {
            design.line_section_a[line] = { line_gains[line], 0.0, 0.0, 0.0, 0.0 };
            design.line_section_b[line] = kUnitySection;
        }
        design.tone_correction = kUnityCascade;
        design.coherence_a = kZeroCascade;
        design.coherence_b = kUnityCascade;
        design.input_gain = 1.0f;
        design.delay_to_crossover_frames = 0.0f;
        return design;
    }

    /// @brief Returns the gains that make every line lose 60 dB in rt60 seconds of
    /// recirculation: 10^(-3 * tau_i / rt60).
    std::array<double, kLines> decay_gains(const std::array<std::uint32_t, kLines>& lines,
                                           double rt60_seconds)
    {
        std::array<double, kLines> gains{};
        for (std::size_t line = 0; line < kLines; ++line)
        {
            const double tau = static_cast<double>(lines[line]) / kSampleRate;
            gains[line] = std::pow(10.0, -3.0 * tau / rt60_seconds);
        }
        return gains;
    }

    /// @brief Returns sixteen copies of one gain.
    std::array<double, kLines> uniform_gains(double gain)
    {
        std::array<double, kLines> gains{};
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
        constexpr std::uint32_t kBlock = 256;
        Rendering out;
        out.left.assign(input.size(), 0.0f);
        out.right.assign(input.size(), 0.0f);
        for (std::size_t start = 0; start < input.size(); start += kBlock)
        {
            const auto frames = static_cast<std::uint32_t>(std::min<std::size_t>(kBlock, input.size() - start));
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
            tail[index] = tail[index + 1]
                + static_cast<double>(response[index]) * static_cast<double>(response[index]);
        }
        const double threshold = tail[0] * 1e-6;
        for (std::size_t index = 0; index < response.size(); ++index)
        {
            if (tail[index] <= threshold)
            {
                return static_cast<double>(index) / kSampleRate;
            }
        }
        return static_cast<double>(response.size()) / kSampleRate;
    }
}

TEST(Unit_Dsp, LateReverbHadamardPreservesEnergy)
{
    // The normalised Hadamard matrix is orthogonal. See tests/functional/unit/README.md.
    std::array<double, kLines> values{};
    double start = 0.0;
    for (std::size_t line = 0; line < kLines; ++line)
    {
        values[line] = std::sin(0.7 * static_cast<double>(line + 1)) + 0.1 * static_cast<double>(line);
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

    // The left ear reads 0 and 4 in turn without decaying. See tests/functional/unit/README.md.
    std::array<std::uint32_t, kLines> ones{};
    ones.fill(1u);
    const FdnDesign design = make_design(ones, uniform_gains(1.0));
    LateReverb reverb;
    reverb.prepare(kMaxLineFrames);
    const Rendering out = render(reverb, design, impulse(1001));
    for (std::size_t frame = 1; frame <= 1000; ++frame)
    {
        const double expected = frame % 2 == 0 ? 4.0 : 0.0;
        EXPECT_NEAR(out.left[frame], expected, 1e-5) << "frame " << frame;
    }
}

TEST(Unit_Dsp, LateReverbDecaysAtTheRateItsFiltersAsk)
{
    const FdnDesign design = make_design(kMidLines, decay_gains(kMidLines, 1.0));
    LateReverb reverb;
    reverb.prepare(kMaxLineFrames);
    const Rendering out = render(reverb, design, impulse(96000));
    EXPECT_NEAR(schroeder_t60(out.left), 1.0, 0.05);
}

TEST(Unit_Dsp, LateReverbIsSilentBeforeItsInputDelay)
{
    FdnDesign design = make_design(kShortLines, uniform_gains(0.5));
    design.delay_to_crossover_frames = 1749.0f;
    LateReverb reverb;
    reverb.prepare(kMaxLineFrames);
    const Rendering out = render(reverb, design, impulse(4096));
    for (std::size_t frame = 0; frame < 1749; ++frame)
    {
        ASSERT_EQ(out.left[frame], 0.0f) << "frame " << frame;
        ASSERT_EQ(out.right[frame], 0.0f) << "frame " << frame;
    }
    // The first line is 293 frames long, so the tail arrives 293 frames after the delay.
    EXPECT_NE(out.left[1749 + 293], 0.0f);
}

TEST(Unit_Dsp, LateReverbEarsDifferButShareTheirEnergy)
{
    const FdnDesign design = make_design(kMidLines, decay_gains(kMidLines, 1.0));
    LateReverb reverb;
    reverb.prepare(kMaxLineFrames);
    const Rendering out = render(reverb, design, impulse(48000));
    EXPECT_NE(out.left, out.right);
    const double ratio_db = 10.0 * std::log10(energy(out.left) / energy(out.right));
    EXPECT_NEAR(ratio_db, 0.0, 0.5);
}

TEST(Unit_Dsp, LateReverbRunsBothSectionsOfEveryOutputFilter)
{
    // Two sections of gain 0.5 scale the output by 0.25. See tests/functional/unit/README.md.
    const FdnDesign plain = make_design(kShortLines, uniform_gains(0.5));
    FdnDesign quarter_tone = plain;
    quarter_tone.tone_correction = half_twice();
    FdnDesign quarter_right = plain;
    quarter_right.coherence_b = half_twice();

    LateReverb reverb;
    reverb.prepare(kMaxLineFrames);
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
        ASSERT_NEAR(tone.right[frame], 0.25f * reference.right[frame], 1e-6f)
            << "frame " << frame;
        ASSERT_EQ(right.left[frame], reference.left[frame]) << "frame " << frame;
        ASSERT_NEAR(right.right[frame], 0.25f * reference.right[frame], 1e-6f)
            << "frame " << frame;
    }
}

TEST(Unit_Dsp, LateReverbResetSilencesTheLines)
{
    const FdnDesign design = make_design(kShortLines, uniform_gains(0.9));
    LateReverb reverb;
    reverb.prepare(kMaxLineFrames);
    render(reverb, design, impulse(4096));
    reverb.reset();
    const Rendering out = render(reverb, design, std::vector<float>(4096, 0.0f));
    for (std::size_t frame = 0; frame < 4096; ++frame)
    {
        ASSERT_EQ(out.left[frame], 0.0f) << "frame " << frame;
        ASSERT_EQ(out.right[frame], 0.0f) << "frame " << frame;
    }
}

TEST(Unit_Dsp, LateReverbSurvivesALineLengthChangeWithoutAClick)
{
    // A 50 Hz sine steps at most 0.0065 of peak per sample. See tests/functional/unit/README.md.
    constexpr std::size_t kBlock = 1024;
    std::vector<float> sine(2 * kBlock);
    for (std::size_t frame = 0; frame < sine.size(); ++frame)
    {
        sine[frame] = static_cast<float>(std::sin(2.0 * kPi * 50.0 * static_cast<double>(frame) / kSampleRate));
    }
    const FdnDesign before = make_design(kShortLines, uniform_gains(0.5));
    const FdnDesign after = make_design(kShorterLines, uniform_gains(0.5));
    LateReverb reverb;
    reverb.prepare(kMaxLineFrames);
    std::vector<float> left(sine.size(), 0.0f);
    std::vector<float> right(sine.size(), 0.0f);
    reverb.process(before, sine.data(), left.data(), right.data(), kBlock);
    reverb.process(after, sine.data() + kBlock, left.data() + kBlock, right.data() + kBlock, kBlock);

    float peak = 0.0f;
    for (std::size_t frame = kBlock; frame < sine.size(); ++frame)
    {
        peak = std::max(peak, std::fabs(left[frame]));
    }
    ASSERT_GT(peak, 0.0f);
    for (std::size_t frame = kBlock; frame < sine.size(); ++frame)
    {
        const float step = std::fabs(left[frame] - left[frame - 1]);
        EXPECT_LE(step, 0.05f * peak) << "frame " << frame;
    }
}
