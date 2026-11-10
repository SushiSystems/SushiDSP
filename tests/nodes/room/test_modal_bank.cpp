/**************************************************************************/
/* test_modal_bank.cpp                                                    */
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
 * @file test_modal_bank.cpp
 * @brief Tests for ModalBank, the resonator bank that renders the room's standing waves.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <numeric>
#include <vector>

#include <SushiDSP/foundation/allocation_trap/allocation_trap.hpp>
#include <SushiDSP/foundation/math/fft.hpp>
#include <SushiDSP/nodes/room/modal_bank.hpp>
#include <SushiDSP/nodes/room/modal_design.hpp>

namespace
{
    using SushiDSP::room::ModalBank;
    using SushiDSP::room::ModalDesign;

    /// @brief Holds pi.
    constexpr double K_PI = 3.14159265358979323846;

    /// @brief Holds the sample rate every test renders at.
    constexpr double K_SAMPLE_RATE = 48000.0;

    /// @brief Holds the block size the renders are split into.
    constexpr std::uint32_t K_BLOCK_FRAMES = 256;

    /// @brief Holds the garage's 2.5 m axial mode.
    constexpr double K_GARAGE_AXIAL_HZ = 68.6;

    /// @brief Holds the reference resonator's 60 dB decay time in seconds.
    constexpr double K_DECAY_SECONDS = 1.0;

    /// @brief Holds how far the peak the spectrum shows may sit from the mode, as a fraction of
    /// the mode. Characterised, not derived: the peak is read by a parabola through three bins
    /// 0.73 Hz apart.
    constexpr double K_PEAK_RELATIVE_TOLERANCE = 0.005;

    /// @brief Holds how far the decay the two windows show may sit from the decay given, as a
    /// fraction of it. Characterised, not derived: each window of 1024 frames holds about 1.5
    /// cycles of the mode, so its RMS ripples by a few percent.
    constexpr double K_DECAY_RELATIVE_TOLERANCE = 0.05;

    /// @brief Holds a rendered stereo pair.
    struct StereoRender
    {
            /// @brief Stores the left ear.
            std::vector<float> left;

            /// @brief Stores the right ear.
            std::vector<float> right;
    };

    /// @brief Writes a resonator at frequency_hz with a 60 dB decay of decay_seconds into mode,
    /// r = 10^(-3 / (T60 fs)), a1 = -2 r cos(theta), a2 = r^2, numerator { 1, 0, -1 }.
    void set_resonator(ModalDesign::Mode& mode, double frequency_hz, double decay_seconds)
    {
        const double radius = std::pow(10.0, -3.0 / (decay_seconds * K_SAMPLE_RATE));
        const double theta = 2.0 * K_PI * frequency_hz / K_SAMPLE_RATE;
        mode.section = {1.0, 0.0, -1.0, -2.0 * radius * std::cos(theta), radius * radius};
        mode.source_gain = 1.0f;
        mode.ear_gain = {1.0f, 1.0f};
    }

    /// @brief Returns a design holding one resonator, the garage's 68.6 Hz mode at a 1.0 s decay:
    /// r = 0.99985610, theta = 0.00897972, a1 = -1.99963157, a2 = 0.99971222.
    std::unique_ptr<ModalDesign> single_mode_design()
    {
        auto design = std::make_unique<ModalDesign>();
        design->count = 1u;
        set_resonator(design->modes[0], K_GARAGE_AXIAL_HZ, K_DECAY_SECONDS);
        return design;
    }

    /// @brief Returns a design of K_MAX_MODES resonators spread from 20 to 500 Hz.
    std::unique_ptr<ModalDesign> full_design()
    {
        constexpr double K_LOWEST_HZ = 20.0;
        constexpr double K_HIGHEST_HZ = 500.0;
        auto design = std::make_unique<ModalDesign>();
        design->count = static_cast<std::uint32_t>(ModalDesign::K_MAX_MODES);
        for (std::size_t index = 0; index < ModalDesign::K_MAX_MODES; ++index)
        {
            const double fraction =
                static_cast<double>(index) / static_cast<double>(ModalDesign::K_MAX_MODES - 1);
            set_resonator(design->modes[index],
                          K_LOWEST_HZ + fraction * (K_HIGHEST_HZ - K_LOWEST_HZ), K_DECAY_SECONDS);
        }
        return design;
    }

    /// @brief Returns a unit impulse at frame 0 followed by silence.
    std::vector<float> impulse(std::size_t frames)
    {
        std::vector<float> signal(frames, 0.0f);
        signal[0] = 1.0f;
        return signal;
    }

    /// @brief Renders a unit impulse through a prepared bank over frames, K_BLOCK_FRAMES at a
    /// time, into zeroed outputs.
    StereoRender render_impulse(ModalBank& bank, const ModalDesign& design, std::size_t frames)
    {
        const std::vector<float> input = impulse(frames);
        StereoRender render{std::vector<float>(frames, 0.0f), std::vector<float>(frames, 0.0f)};
        for (std::size_t start = 0; start < frames; start += K_BLOCK_FRAMES)
        {
            const auto count =
                static_cast<std::uint32_t>(std::min<std::size_t>(K_BLOCK_FRAMES, frames - start));
            bank.process(design, input.data() + start, render.left.data() + start,
                         render.right.data() + start, count);
        }
        return render;
    }

    /// @brief Renders a unit impulse through a bank prepared and used once.
    StereoRender render_fresh(const ModalDesign& design, std::size_t frames)
    {
        ModalBank bank;
        bank.prepare(K_BLOCK_FRAMES);
        return render_impulse(bank, design, frames);
    }

    /// @brief Returns the RMS of window_frames samples starting at start.
    double window_rms(const std::vector<float>& signal, std::size_t start,
                      std::size_t window_frames)
    {
        double sum = 0.0;
        for (std::size_t frame = start; frame < start + window_frames; ++frame)
        {
            sum += static_cast<double>(signal[frame]) * static_cast<double>(signal[frame]);
        }
        return std::sqrt(sum / static_cast<double>(window_frames));
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

    /// @brief Returns the frequency of the spectrum's peak, placed between bins by a parabola
    /// through the three bins around the maximum.
    double peak_frequency_hz(const std::vector<float>& signal)
    {
        const std::uint32_t size = static_cast<std::uint32_t>(signal.size());
        SushiDSP::Fft fft(size);
        std::vector<float> real(size);
        std::vector<float> imaginary(size);
        fft.forward(signal.data(), real.data(), imaginary.data());
        std::vector<double> power_db(size / 2 + 1, 0.0);
        std::size_t peak_bin = 1;
        for (std::size_t bin = 1; bin <= size / 2; ++bin)
        {
            const double power =
                static_cast<double>(real[bin]) * static_cast<double>(real[bin]) +
                static_cast<double>(imaginary[bin]) * static_cast<double>(imaginary[bin]);
            power_db[bin] = 10.0 * std::log10(power);
            peak_bin = power_db[bin] > power_db[peak_bin] ? bin : peak_bin;
        }
        const double below = power_db[peak_bin - 1];
        const double at = power_db[peak_bin];
        const double above = power_db[peak_bin + 1];
        const double offset = 0.5 * (below - above) / (below - 2.0 * at + above);
        return (static_cast<double>(peak_bin) + offset) * K_SAMPLE_RATE / size;
    }

    /// @brief Returns the 60 dB time the slope between an early and a late window of the
    /// response shows, in seconds.
    double measured_decay_seconds(const std::vector<float>& response)
    {
        constexpr std::size_t K_WINDOW_FRAMES = 1024;
        constexpr std::size_t K_EARLY_WINDOW = 5;
        constexpr std::size_t K_LATE_WINDOW = 40;
        const double early_db =
            20.0 * std::log10(window_rms(response, K_EARLY_WINDOW * K_WINDOW_FRAMES,
                                         K_WINDOW_FRAMES));
        const double late_db =
            20.0 * std::log10(window_rms(response, K_LATE_WINDOW * K_WINDOW_FRAMES,
                                         K_WINDOW_FRAMES));
        const double slope_db_per_frame =
            (early_db - late_db) /
            static_cast<double>((K_LATE_WINDOW - K_EARLY_WINDOW) * K_WINDOW_FRAMES);
        return 60.0 / slope_db_per_frame / K_SAMPLE_RATE;
    }

    /// @brief Counts the subnormal samples of both ears.
    std::size_t subnormal_count(const StereoRender& render)
    {
        const auto is_subnormal = [](float sample)
        { return std::fpclassify(sample) == FP_SUBNORMAL; };
        return static_cast<std::size_t>(
            std::count_if(render.left.begin(), render.left.end(), is_subnormal) +
            std::count_if(render.right.begin(), render.right.end(), is_subnormal));
    }

    /// @brief Returns whether every sample is finite.
    bool all_finite(const std::vector<float>& signal)
    {
        return std::all_of(signal.begin(), signal.end(),
                           [](float sample) { return std::isfinite(sample); });
    }

    /// @brief Returns a block of a 220 Hz tone at 0.3 of full scale.
    std::vector<float> room_tone(std::size_t frames)
    {
        std::vector<float> block(frames);
        for (std::size_t i = 0; i < frames; ++i)
        {
            block[i] = 0.3f * std::sin(2.0f * static_cast<float>(K_PI) * 220.0f *
                                       static_cast<float>(i) / 48000.0f);
        }
        return block;
    }

    /// @brief Returns a design of K_MAX_MODES resonators all at 68.6 Hz with a 1 s decay, which
    /// the allocation trap runs.
    std::unique_ptr<ModalDesign> trapped_design()
    {
        auto design = std::make_unique<ModalDesign>();
        const double radius = std::pow(10.0, -3.0 / 48000.0);
        const double theta = 2.0 * K_PI * 68.6 / 48000.0;
        design->count = static_cast<std::uint32_t>(ModalDesign::K_MAX_MODES);
        for (std::uint32_t mode = 0; mode < design->count; ++mode)
        {
            design->modes[mode].section = {1e-3, 0.0, -1e-3, -2.0 * radius * std::cos(theta),
                                           radius * radius};
            design->modes[mode].source_gain = 0.5f;
            design->modes[mode].ear_gain = {0.7f, -0.7f};
        }
        design->crossover_hz = 278.18f;
        design->level_scale = 1.0f;
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

    /// @brief Runs eight blocks of 256 frames, each followed by a block of one frame, and resets.
    void run_blocks(ModalBank& bank, const ModalDesign& design, const std::vector<float>& input,
                    TrappedBlocks& out)
    {
        for (int block = 0; block < 8; ++block)
        {
            bank.process(design, input.data(), out.left.data(), out.right.data(), 256);
            bank.process(design, input.data(), out.left.data(), out.right.data(), 1);
        }
        bank.reset();
    }
} // namespace

TEST(Small_ModalBank, RingsAtEachModesFrequency)
{
    // A parabola through three bins places the peak. See tests/nodes/room/README.md.
    constexpr std::size_t K_FFT_SIZE = 65536;
    const StereoRender render = render_fresh(*single_mode_design(), K_FFT_SIZE);
    EXPECT_NEAR(peak_frequency_hz(render.left), K_GARAGE_AXIAL_HZ,
                K_PEAK_RELATIVE_TOLERANCE * K_GARAGE_AXIAL_HZ);
}

TEST(Small_ModalBank, DecaysAtTheDampingItWasGiven)
{
    // The slope between two windows is the 60 dB time. See tests/nodes/room/README.md.
    const StereoRender render = render_fresh(*single_mode_design(), 96000);
    EXPECT_NEAR(measured_decay_seconds(render.left), K_DECAY_SECONDS,
                K_DECAY_RELATIVE_TOLERANCE * K_DECAY_SECONDS);
}

TEST(Small_ModalBank, EarGainSignsTheRightEar)
{
    constexpr std::size_t K_FRAMES = 4096;
    const auto design = single_mode_design();
    design->modes[0].ear_gain = {1.0f, -1.0f};
    const StereoRender render = render_fresh(*design, K_FRAMES);
    ASSERT_GT(window_rms(render.left, 0, K_FRAMES), 0.0);
    EXPECT_EQ(render.right, scaled(render.left, -1.0f));
}

TEST(Small_ModalBank, SourceGainScalesTheOutput)
{
    // A source gain of 0.5 halves the render exactly. See tests/nodes/room/README.md.
    constexpr std::size_t K_FRAMES = 4096;
    const StereoRender unit = render_fresh(*single_mode_design(), K_FRAMES);
    const auto half_design = single_mode_design();
    half_design->modes[0].source_gain = 0.5f;
    const StereoRender half = render_fresh(*half_design, K_FRAMES);
    ASSERT_GT(window_rms(unit.left, 0, K_FRAMES), 0.0);
    EXPECT_EQ(half.left, scaled(unit.left, 0.5f));
}

TEST(Medium_ModalBank, RunsFiveHundredAndTwelveModesWithoutDenormals)
{
    const StereoRender render = render_fresh(*full_design(), 96000);
    ASSERT_NE(render.left[1], 0.0f);
    EXPECT_EQ(subnormal_count(render), 0u);
}

TEST(Small_ModalBank, WithNoModesAddsNothing)
{
    constexpr std::uint32_t K_FRAMES = 512;
    auto design = std::make_unique<ModalDesign>();
    design->count = 0u;
    ModalBank bank;
    bank.prepare(K_BLOCK_FRAMES);

    std::vector<float> input(K_FRAMES, 1.0f);
    std::vector<float> left(K_FRAMES);
    std::vector<float> right(K_FRAMES);
    std::iota(left.begin(), left.end(), 0.25f);
    std::iota(right.begin(), right.end(), -0.75f);
    const std::vector<float> left_before = left;
    const std::vector<float> right_before = right;
    bank.process(*design, input.data(), left.data(), right.data(), K_FRAMES);

    EXPECT_EQ(left, left_before);
    EXPECT_EQ(right, right_before);
}

TEST(Small_ModalBank, ResetSilencesEveryResonator)
{
    constexpr std::uint32_t K_FRAMES = 4096;
    const auto design = single_mode_design();
    ModalBank bank;
    bank.prepare(K_BLOCK_FRAMES);
    const StereoRender ringing = render_impulse(bank, *design, K_FRAMES);
    ASSERT_NE(ringing.left[K_FRAMES - 1], 0.0f);

    bank.reset();
    const std::vector<float> silence(K_FRAMES, 0.0f);
    std::vector<float> left(K_FRAMES, 0.0f);
    std::vector<float> right(K_FRAMES, 0.0f);
    bank.process(*design, silence.data(), left.data(), right.data(), K_FRAMES);
    EXPECT_EQ(left, silence);
    EXPECT_EQ(right, silence);
}

class Small_ModalBankRows : public ::testing::Test
{
    protected:
        /// @brief Renders seven modes, which spill into a second lane group, as two ears and as
        /// three read-out rows: left, right and left again.
        void SetUp() override
        {
            design = std::make_unique<ModalDesign>();
            design->count = 7u;
            std::vector<float> left_gains(ModalDesign::K_MAX_MODES, 0.0f);
            std::vector<float> right_gains(ModalDesign::K_MAX_MODES, 0.0f);
            for (std::uint32_t index = 0; index < design->count; ++index)
            {
                set_resonator(design->modes[index], 40.0 + 23.0 * index, 0.4 + 0.1 * index);
                design->modes[index].source_gain = 0.3f + 0.1f * static_cast<float>(index);
                design->modes[index].ear_gain = {1.0f - 0.2f * static_cast<float>(index),
                                                 -0.5f + 0.15f * static_cast<float>(index)};
                left_gains[index] = design->modes[index].ear_gain[0];
                right_gains[index] = design->modes[index].ear_gain[1];
            }

            ModalBank two_ears;
            two_ears.prepare(K_BLOCK_FRAMES);
            reference = render_impulse(two_ears, *design, K_FRAMES);

            ModalBank rows;
            rows.prepare(K_BLOCK_FRAMES);
            const std::vector<float> input = impulse(K_FRAMES);
            first.assign(K_FRAMES, 0.0f);
            second.assign(K_FRAMES, 0.0f);
            third.assign(K_FRAMES, 0.0f);
            for (std::size_t start = 0; start < K_FRAMES; start += K_BLOCK_FRAMES)
            {
                const ModalBank::ReadOut read_outs[3] = {
                    {left_gains.data(), first.data() + start},
                    {right_gains.data(), second.data() + start},
                    {left_gains.data(), third.data() + start},
                };
                rows.process(*design, input.data() + start, read_outs, 3u, K_BLOCK_FRAMES);
            }
            ASSERT_GT(window_rms(first, 0, K_FRAMES), 0.0);
        }

        /// @brief Counts the frames rendered.
        static constexpr std::size_t K_FRAMES = 4 * K_BLOCK_FRAMES;

        /// @brief Stores the seven modes.
        std::unique_ptr<ModalDesign> design;

        /// @brief Stores the two-ear render.
        StereoRender reference;

        /// @brief Stores the first read-out row, the left gains.
        std::vector<float> first;

        /// @brief Stores the second read-out row, the right gains.
        std::vector<float> second;

        /// @brief Stores the third read-out row, the left gains again.
        std::vector<float> third;
};

TEST_F(Small_ModalBankRows, ReadsTheLeftGainsAsTheLeftEar)
{
    // Seven modes spill into a second lane group, so a row is summed across groups too.
    EXPECT_EQ(first, reference.left);
}

TEST_F(Small_ModalBankRows, ReadsTheRightGainsAsTheRightEar)
{
    EXPECT_EQ(second, reference.right);
}

TEST_F(Small_ModalBankRows, ARepeatedRowEqualsTheFirst)
{
    EXPECT_EQ(third, first);
}

TEST(Small_ModalBank, OutputStaysFiniteAcrossBlocksOfEveryLength)
{
    ModalBank bank;
    bank.prepare(256);
    const auto design = trapped_design();
    const std::vector<float> input = room_tone(256);
    TrappedBlocks out;
    run_blocks(bank, *design, input, out);
    EXPECT_TRUE(all_finite(out.left));
    EXPECT_TRUE(all_finite(out.right));
}

TEST(Medium_ModalBank, ProcessPerformsNoHeapAllocation)
{
    // All K_MAX_MODES resonators run at 68.6 Hz, 1 s decay. See tests/nodes/room/README.md.
    ModalBank bank;
    bank.prepare(256);
    const auto design = trapped_design();
    const std::vector<float> input = room_tone(256);
    TrappedBlocks out;

    SushiDSP::test::AllocationTrap trap;
    trap.arm();
    run_blocks(bank, *design, input, out);
    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
}
