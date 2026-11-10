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

#include "support/tolerance.hpp"

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

    /// @brief Holds the input delay the silence tests ask for, in frames.
    constexpr std::size_t K_INPUT_DELAY_FRAMES = 1749;

    /// @brief Counts the Hadamard transforms the energy test applies in turn.
    constexpr int K_HADAMARD_PASSES = 1000;

    /// @brief Holds how far the Schroeder estimate of a 1 s decay may sit from 1 s, in seconds.
    /// Characterised, not derived: the estimate reads a mixed sixteen-line tail whose mode beating
    /// the design does not predict.
    constexpr double K_T60_TOLERANCE_SECONDS = 0.05;

    /// @brief Holds how far the two ears' energies may differ over a full decay, in decibels.
    /// Characterised, not derived: it is the spread of the sixteen-line tail's energy between the
    /// A and B directions, which depends on the line lengths.
    constexpr double K_EAR_ENERGY_TOLERANCE_DB = 0.5;

    /// @brief Holds the largest sample-to-sample step the click test allows, as a fraction of the
    /// peak. Characterised, not derived: a 50 Hz sine steps 2 pi 50 / 48000 = 0.0065 of its peak,
    /// and the output is a sum of such sines through filters that change its amplitude.
    constexpr float K_MAX_STEP_OF_PEAK = 0.05f;

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

    /// @brief Returns the impulse through a freshly prepared network, rendered over frames frames.
    Rendering impulse_response(const FdnDesign& design, std::size_t frames)
    {
        LateReverb reverb;
        reverb.prepare(K_MAX_LINE_FRAMES);
        std::vector<float> input(frames, 0.0f);
        input[0] = 1.0f;
        return render(reverb, design, input);
    }

    /// @brief Returns a signal scaled by a factor.
    std::vector<float> scaled(const std::vector<float>& signal, float factor)
    {
        std::vector<float> out(signal);
        for (float& sample : out)
        {
            sample *= factor;
        }
        return out;
    }

    /// @brief Returns the index of the first sample that is not zero, or the size when all are.
    std::size_t first_nonzero(const std::vector<float>& signal)
    {
        const auto found = std::find_if(signal.begin(), signal.end(),
                                        [](float sample) { return sample != 0.0f; });
        return static_cast<std::size_t>(found - signal.begin());
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

    /// @brief Returns the sum of squares of sixteen values.
    double energy_of(const std::array<double, K_LINES>& values)
    {
        double sum = 0.0;
        for (const double value : values)
        {
            sum += value * value;
        }
        return sum;
    }

    /// @brief Returns the energy of a fixed vector after the given number of Hadamard transforms,
    /// over its energy before.
    double hadamard_energy_ratio(int passes)
    {
        std::array<double, K_LINES> values{};
        for (std::size_t line = 0; line < K_LINES; ++line)
        {
            values[line] =
                std::sin(0.7 * static_cast<double>(line + 1)) + 0.1 * static_cast<double>(line);
        }
        const double start = energy_of(values);
        for (int pass = 0; pass < passes; ++pass)
        {
            LateReverb::hadamard_transform(values);
        }
        return energy_of(values) / start;
    }

    /// @brief Returns the bound on how far the energy of a vector drifts from its start over the
    /// given number of Hadamard passes. A pass has four butterfly stages. Each stage's rounding
    /// error is at most u of its output norm, and the remaining stages and the scale of 1/4
    /// carry it to u of the input norm at the end, so a pass moves the norm by 4u and the
    /// energy by 8u. The two sums of sixteen squares add 16u each.
    double hadamard_energy_bound(int passes)
    {
        return (8.0 * static_cast<double>(passes) + 32.0) *
               TestSupport::unit_roundoff<double>();
    }

    /// @brief Returns a design of unit lines one frame long, so the tail alternates between all
    /// ones and 4 on line 0 without decay.
    FdnDesign unit_lines_design()
    {
        std::array<std::uint32_t, K_LINES> ones{};
        ones.fill(1u);
        return make_design(ones, uniform_gains(1.0));
    }

    /// @brief Returns frames samples that read 4 on every even frame after 0 and 0 elsewhere.
    std::vector<float> alternating_four(std::size_t frames)
    {
        std::vector<float> expected(frames, 0.0f);
        for (std::size_t frame = 2; frame < frames; frame += 2)
        {
            expected[frame] = 4.0f;
        }
        return expected;
    }

    /// @brief Returns a sine of the given frequency and amplitude.
    std::vector<float> sine(std::size_t frames, double frequency, double amplitude)
    {
        std::vector<float> signal(frames);
        for (std::size_t frame = 0; frame < frames; ++frame)
        {
            signal[frame] = static_cast<float>(
                amplitude *
                std::sin(2.0 * K_PI * frequency * static_cast<double>(frame) / K_SAMPLE_RATE));
        }
        return signal;
    }

    /// @brief Returns the largest absolute sample from a frame on.
    float peak_from(const std::vector<float>& signal, std::size_t from)
    {
        float peak = 0.0f;
        for (std::size_t frame = from; frame < signal.size(); ++frame)
        {
            peak = std::max(peak, std::fabs(signal[frame]));
        }
        return peak;
    }

    /// @brief Returns the largest absolute step between neighbouring samples from a frame on.
    float largest_step_from(const std::vector<float>& signal, std::size_t from)
    {
        float step = 0.0f;
        for (std::size_t frame = from; frame < signal.size(); ++frame)
        {
            step = std::max(step, std::fabs(signal[frame] - signal[frame - 1]));
        }
        return step;
    }

    /// @brief Returns whether every sample is finite.
    bool all_finite(const std::vector<float>& signal)
    {
        return std::all_of(signal.begin(), signal.end(),
                           [](float sample) { return std::isfinite(sample); });
    }

    /// @brief Returns a design of 1000 + 97 line frames, damped and mixed, for the blocks the
    /// allocation trap watches.
    FdnDesign trapped_design()
    {
        FdnDesign design{};
        const FdnDesign::Section damped = {0.9, 0.0, 0.0, 0.0, 0.0};
        for (std::size_t line = 0; line < K_LINES; ++line)
        {
            design.line_frames[line] = static_cast<std::uint32_t>(1000 + 97 * line);
            design.line_section_a[line] = damped;
            design.line_section_b[line] = K_UNITY_SECTION;
        }
        design.tone_correction = K_UNITY_CASCADE;
        design.coherence_a = K_UNITY_CASCADE;
        design.coherence_b = K_UNITY_CASCADE;
        design.input_gain = 0.5f;
        design.delay_to_crossover_frames = 1749.0f;
        return design;
    }

    /// @brief Holds the output of the blocks the allocation trap watches.
    struct TrappedBlocks
    {
            /// @brief Stores the left ear of the last block.
            std::vector<float> left = std::vector<float>(256, 0.0f);

            /// @brief Stores the right ear of the last block.
            std::vector<float> right = std::vector<float>(256, 0.0f);
    };

    /// @brief Renders eight blocks of 256 frames, the first four at the design and the last four
    /// at the design with every line 37 frames longer, each block once for two ears and once
    /// for one buffer.
    void run_blocks(LateReverb& reverb, const std::vector<float>& input, TrappedBlocks& out)
    {
        const FdnDesign design = trapped_design();
        FdnDesign moved = design;
        for (std::uint32_t& frames : moved.line_frames)
        {
            frames += 37u;
        }
        for (int block = 0; block < 8; ++block)
        {
            const FdnDesign& current = block < 4 ? design : moved;
            reverb.process(current, input.data(), out.left.data(), out.right.data(), 256);
            reverb.process(current, input.data(), out.left.data(), out.left.data(), 256);
        }
    }
} // namespace

TEST(Small_LateReverb, HadamardPreservesEnergy)
{
    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(
        hadamard_energy_ratio(K_HADAMARD_PASSES), 1.0, hadamard_energy_bound(K_HADAMARD_PASSES))));
}

TEST(Small_LateReverb, UnitLinesCarryAnImpulseToTheLeftEarEveryOtherFrameWithoutDecay)
{
    // The left ear reads 0 and 4 in turn without decaying. See tests/nodes/room/README.md.
    const Rendering out = impulse_response(unit_lines_design(), 1001);
    EXPECT_EQ(out.left, alternating_four(1001));
}

TEST(Small_LateReverb, DecaysAtTheRateItsFiltersAsk)
{
    const FdnDesign design = make_design(K_MID_LINES, decay_gains(K_MID_LINES, 1.0));
    const Rendering out = impulse_response(design, 96000);
    EXPECT_NEAR(schroeder_t60(out.left), 1.0, K_T60_TOLERANCE_SECONDS);
}

TEST(Small_LateReverb, IsSilentBeforeItsInputDelay)
{
    FdnDesign design = make_design(K_SHORT_LINES, uniform_gains(0.5));
    design.delay_to_crossover_frames = static_cast<float>(K_INPUT_DELAY_FRAMES);
    const Rendering out = impulse_response(design, 4096);
    EXPECT_GE(first_nonzero(out.left), K_INPUT_DELAY_FRAMES);
    EXPECT_GE(first_nonzero(out.right), K_INPUT_DELAY_FRAMES);
}

TEST(Small_LateReverb, TailArrivesOneShortestLineAfterTheInputDelay)
{
    // The first line is 293 frames long, so the tail arrives 293 frames after the delay.
    FdnDesign design = make_design(K_SHORT_LINES, uniform_gains(0.5));
    design.delay_to_crossover_frames = static_cast<float>(K_INPUT_DELAY_FRAMES);
    const Rendering out = impulse_response(design, 4096);
    EXPECT_EQ(first_nonzero(out.left), K_INPUT_DELAY_FRAMES + K_SHORT_LINES.front());
    EXPECT_EQ(first_nonzero(out.right), K_INPUT_DELAY_FRAMES + K_SHORT_LINES.front());
}

TEST(Small_LateReverb, EarsDiffer)
{
    const FdnDesign design = make_design(K_MID_LINES, decay_gains(K_MID_LINES, 1.0));
    const Rendering out = impulse_response(design, 48000);
    EXPECT_NE(out.left, out.right);
}

TEST(Small_LateReverb, EarsShareTheirEnergy)
{
    const FdnDesign design = make_design(K_MID_LINES, decay_gains(K_MID_LINES, 1.0));
    const Rendering out = impulse_response(design, 48000);
    EXPECT_NEAR(10.0 * std::log10(energy(out.left) / energy(out.right)), 0.0,
                K_EAR_ENERGY_TOLERANCE_DB);
}

class Small_LateReverbOutputFilters : public ::testing::Test
{
    protected:
        /// @brief Renders the plain design, which every filter below is compared with.
        void SetUp() override
        {
            plain = make_design(K_SHORT_LINES, uniform_gains(0.5));
            reference = impulse_response(plain, 4096);
            ASSERT_GT(energy(reference.left), 0.0);
            ASSERT_GT(energy(reference.right), 0.0);
        }

        /// @brief Stores the design with unity output filters and the left ear reading A alone.
        FdnDesign plain{};

        /// @brief Stores the rendering of the plain design.
        Rendering reference;
};

TEST_F(Small_LateReverbOutputFilters, ToneCorrectionRunsBothSectionsOnBothEars)
{
    // Two sections of gain 0.5 scale the output by 0.25. See tests/nodes/room/README.md.
    FdnDesign design = plain;
    design.tone_correction = half_twice();
    const Rendering out = impulse_response(design, 4096);
    EXPECT_EQ(out.left, scaled(reference.left, 0.25f));
    EXPECT_EQ(out.right, scaled(reference.right, 0.25f));
}

TEST_F(Small_LateReverbOutputFilters, CoherenceBRunsBothSectionsOnTheRightEar)
{
    FdnDesign design = plain;
    design.coherence_b = half_twice();
    const Rendering out = impulse_response(design, 4096);
    EXPECT_EQ(out.right, scaled(reference.right, 0.25f));
}

TEST_F(Small_LateReverbOutputFilters, CoherenceBLeavesTheLeftEarAlone)
{
    FdnDesign design = plain;
    design.coherence_b = half_twice();
    const Rendering out = impulse_response(design, 4096);
    EXPECT_EQ(out.left, reference.left);
}

TEST(Small_LateReverb, ResetSilencesTheLines)
{
    const FdnDesign design = make_design(K_SHORT_LINES, uniform_gains(0.9));
    LateReverb reverb;
    reverb.prepare(K_MAX_LINE_FRAMES);
    std::vector<float> impulse(4096, 0.0f);
    impulse[0] = 1.0f;
    render(reverb, design, impulse);
    reverb.reset();
    const Rendering out = render(reverb, design, std::vector<float>(4096, 0.0f));
    EXPECT_EQ(out.left, std::vector<float>(4096, 0.0f));
    EXPECT_EQ(out.right, std::vector<float>(4096, 0.0f));
}

TEST(Small_LateReverb, SurvivesALineLengthChangeWithoutAClick)
{
    // A 50 Hz sine steps at most 0.0065 of peak per sample. See tests/nodes/room/README.md.
    constexpr std::size_t K_BLOCK = 1024;
    const std::vector<float> input = sine(2 * K_BLOCK, 50.0, 1.0);
    const FdnDesign before = make_design(K_SHORT_LINES, uniform_gains(0.5));
    const FdnDesign after = make_design(K_SHORTER_LINES, uniform_gains(0.5));
    LateReverb reverb;
    reverb.prepare(K_MAX_LINE_FRAMES);
    std::vector<float> left(input.size(), 0.0f);
    std::vector<float> right(input.size(), 0.0f);
    reverb.process(before, input.data(), left.data(), right.data(), K_BLOCK);
    reverb.process(after, input.data() + K_BLOCK, left.data() + K_BLOCK, right.data() + K_BLOCK,
                   K_BLOCK);

    const float peak = peak_from(left, K_BLOCK);
    ASSERT_GT(peak, 0.0f);
    EXPECT_LE(largest_step_from(left, K_BLOCK), K_MAX_STEP_OF_PEAK * peak);
}

TEST(Small_LateReverb, ProcessSumsReturnsTheRawSumsBeforeTheToneAndCoherenceFilters)
{
    // Only line 1 passes its section, so the sums read A[1] and B[1]. See tests/nodes/room/README.md.
    FdnDesign design = make_design(unit_lines_design().line_frames, uniform_gains(0.0));
    design.line_section_a[1] = K_UNITY_SECTION;
    design.tone_correction = half_twice();
    design.coherence_a = half_twice();
    design.coherence_b = half_twice();
    design.input_gain = 0.5f;
    design.delay_to_crossover_frames = 100.0f;
    LateReverb reverb;
    reverb.prepare(K_MAX_LINE_FRAMES);
    const std::vector<float> input = {1.0f, 0.0f, 0.0f};
    std::vector<float> sum_a(3, 0.0f);
    std::vector<float> sum_b(3, 0.0f);
    reverb.process_sums(design, input.data(), sum_a.data(), sum_b.data(), 3);
    EXPECT_EQ(sum_a, (std::vector<float>{0.0f, 1.0f, -0.25f}));
    EXPECT_EQ(sum_b, (std::vector<float>{0.0f, -1.0f, 0.25f}));
}

TEST(Small_LateReverb, OutputStaysFiniteThroughALengthCrossfade)
{
    LateReverb reverb;
    reverb.prepare(25200);
    const std::vector<float> input = sine(256, 220.0, 0.3);
    TrappedBlocks out;
    run_blocks(reverb, input, out);
    EXPECT_TRUE(all_finite(out.left));
    EXPECT_TRUE(all_finite(out.right));
}

TEST(Medium_LateReverb, ProcessPerformsNoHeapAllocation)
{
    // The trapped blocks include the 512-frame crossfade. See tests/nodes/room/README.md.
    LateReverb reverb;
    reverb.prepare(25200);
    const std::vector<float> input = sine(256, 220.0, 0.3);
    TrappedBlocks out;

    SushiDSP::test::AllocationTrap trap;
    trap.arm();
    run_blocks(reverb, input, out);
    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
}
