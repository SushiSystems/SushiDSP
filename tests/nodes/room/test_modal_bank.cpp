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
 * @file test_modal_bank.cpp
 * @brief Tests for ModalBank, the resonator bank that renders the room's standing waves.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include <SushiDSP/foundation/math/fft.hpp>
#include <SushiDSP/nodes/room/modal_bank.hpp>

namespace
{
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
    void set_resonator(SushiDSP::room::ModalDesign::Mode& mode, double frequency_hz,
                       double decay_seconds)
    {
        const double radius = std::pow(10.0, -3.0 / (decay_seconds * K_SAMPLE_RATE));
        const double theta = 2.0 * K_PI * frequency_hz / K_SAMPLE_RATE;
        mode.section = {1.0, 0.0, -1.0, -2.0 * radius * std::cos(theta), radius * radius};
        mode.source_gain = 1.0f;
        mode.ear_gain = {1.0f, 1.0f};
    }

    /// @brief Returns a design holding one resonator, the garage's 68.6 Hz mode at a 1.0 s decay:
    /// r = 0.99985610, theta = 0.00897972, a1 = -1.99963157, a2 = 0.99971222.
    std::unique_ptr<SushiDSP::room::ModalDesign> single_mode_design()
    {
        auto design = std::make_unique<SushiDSP::room::ModalDesign>();
        design->count = 1u;
        set_resonator(design->modes[0], K_GARAGE_AXIAL_HZ, K_DECAY_SECONDS);
        return design;
    }

    /// @brief Renders a unit impulse through a prepared bank over frames, K_BLOCK_FRAMES at a
    /// time, into zeroed outputs.
    StereoRender render_impulse(SushiDSP::room::ModalBank& bank,
                                const SushiDSP::room::ModalDesign& design, std::size_t frames)
    {
        std::vector<float> input(frames, 0.0f);
        input[0] = 1.0f;
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
} // namespace

TEST(ModalBank, ModalBankRingsAtEachModesFrequency)
{
    constexpr std::uint32_t K_FFT_SIZE = 65536;
    const auto design = single_mode_design();
    SushiDSP::room::ModalBank bank;
    bank.prepare(K_BLOCK_FRAMES);
    const StereoRender render = render_impulse(bank, *design, K_FFT_SIZE);

    SushiDSP::Fft fft(K_FFT_SIZE);
    std::vector<float> real(K_FFT_SIZE);
    std::vector<float> imaginary(K_FFT_SIZE);
    fft.forward(render.left.data(), real.data(), imaginary.data());
    std::vector<double> power_db(K_FFT_SIZE / 2 + 1, 0.0);
    std::size_t peak_bin = 1;
    for (std::size_t bin = 1; bin <= K_FFT_SIZE / 2; ++bin)
    {
        const double power =
            static_cast<double>(real[bin]) * static_cast<double>(real[bin]) +
            static_cast<double>(imaginary[bin]) * static_cast<double>(imaginary[bin]);
        power_db[bin] = 10.0 * std::log10(power);
        peak_bin = power_db[bin] > power_db[peak_bin] ? bin : peak_bin;
    }
    // A parabola through three bins places the peak. See tests/nodes/room/README.md.
    const double below = power_db[peak_bin - 1];
    const double at = power_db[peak_bin];
    const double above = power_db[peak_bin + 1];
    const double offset = 0.5 * (below - above) / (below - 2.0 * at + above);
    const double peak_hz = (static_cast<double>(peak_bin) + offset) * K_SAMPLE_RATE / K_FFT_SIZE;
    EXPECT_NEAR(peak_hz, K_GARAGE_AXIAL_HZ, 0.005 * K_GARAGE_AXIAL_HZ);
}

TEST(ModalBank, ModalBankDecaysAtTheDampingItWasGiven)
{
    // The slope between two windows is the 60 dB time. See tests/nodes/room/README.md.
    constexpr std::size_t K_FRAMES = 96000;
    constexpr std::size_t K_WINDOW_FRAMES = 1024;
    constexpr std::size_t K_EARLY_WINDOW = 5;
    constexpr std::size_t K_LATE_WINDOW = 40;
    const auto design = single_mode_design();
    SushiDSP::room::ModalBank bank;
    bank.prepare(K_BLOCK_FRAMES);
    const StereoRender render = render_impulse(bank, *design, K_FRAMES);

    const double early_db =
        20.0 *
        std::log10(window_rms(render.left, K_EARLY_WINDOW * K_WINDOW_FRAMES, K_WINDOW_FRAMES));
    const double late_db =
        20.0 *
        std::log10(window_rms(render.left, K_LATE_WINDOW * K_WINDOW_FRAMES, K_WINDOW_FRAMES));
    const double slope_db_per_frame =
        (early_db - late_db) /
        static_cast<double>((K_LATE_WINDOW - K_EARLY_WINDOW) * K_WINDOW_FRAMES);
    const double decay_seconds = 60.0 / slope_db_per_frame / K_SAMPLE_RATE;
    EXPECT_NEAR(decay_seconds, K_DECAY_SECONDS, 0.05 * K_DECAY_SECONDS);
}

TEST(ModalBank, ModalBankScalesEachModeByItsSourceAndEarGains)
{
    constexpr std::size_t K_FRAMES = 4096;
    const auto unit_design = single_mode_design();
    SushiDSP::room::ModalBank unit_bank;
    unit_bank.prepare(K_BLOCK_FRAMES);
    const StereoRender unit = render_impulse(unit_bank, *unit_design, K_FRAMES);

    const auto scaled_design = single_mode_design();
    scaled_design->modes[0].source_gain = 0.5f;
    scaled_design->modes[0].ear_gain = {1.0f, -1.0f};
    SushiDSP::room::ModalBank scaled_bank;
    scaled_bank.prepare(K_BLOCK_FRAMES);
    const StereoRender scaled = render_impulse(scaled_bank, *scaled_design, K_FRAMES);

    for (std::size_t frame = 0; frame < K_FRAMES; ++frame)
    {
        EXPECT_EQ(scaled.right[frame], -scaled.left[frame]) << "frame " << frame;
        EXPECT_NEAR(scaled.left[frame], 0.5f * unit.left[frame], 1e-7f) << "frame " << frame;
    }
}

TEST(ModalBank, ModalBankRunsFiveHundredAndTwelveModesWithoutDenormals)
{
    constexpr std::size_t K_FRAMES = 96000;
    constexpr double K_LOWEST_HZ = 20.0;
    constexpr double K_HIGHEST_HZ = 500.0;
    auto design = std::make_unique<SushiDSP::room::ModalDesign>();
    design->count = static_cast<std::uint32_t>(SushiDSP::room::ModalDesign::K_MAX_MODES);
    for (std::size_t index = 0; index < SushiDSP::room::ModalDesign::K_MAX_MODES; ++index)
    {
        const double fraction = static_cast<double>(index) /
                                static_cast<double>(SushiDSP::room::ModalDesign::K_MAX_MODES - 1);
        set_resonator(design->modes[index], K_LOWEST_HZ + fraction * (K_HIGHEST_HZ - K_LOWEST_HZ),
                      K_DECAY_SECONDS);
    }
    SushiDSP::room::ModalBank bank;
    bank.prepare(K_BLOCK_FRAMES);
    const StereoRender render = render_impulse(bank, *design, K_FRAMES);

    std::size_t denormal_count = 0;
    for (std::size_t frame = 0; frame < K_FRAMES; ++frame)
    {
        denormal_count += std::fpclassify(render.left[frame]) == FP_SUBNORMAL ? 1u : 0u;
        denormal_count += std::fpclassify(render.right[frame]) == FP_SUBNORMAL ? 1u : 0u;
    }
    EXPECT_EQ(denormal_count, 0u);
    EXPECT_NE(render.left[1], 0.0f);
}

TEST(ModalBank, ModalBankWithNoModesAddsNothing)
{
    constexpr std::uint32_t K_FRAMES = 512;
    auto design = std::make_unique<SushiDSP::room::ModalDesign>();
    design->count = 0u;
    SushiDSP::room::ModalBank bank;
    bank.prepare(K_BLOCK_FRAMES);

    std::vector<float> input(K_FRAMES, 1.0f);
    std::vector<float> left(K_FRAMES);
    std::vector<float> right(K_FRAMES);
    for (std::uint32_t frame = 0; frame < K_FRAMES; ++frame)
    {
        left[frame] = 0.25f + static_cast<float>(frame);
        right[frame] = -0.75f - static_cast<float>(frame);
    }
    const std::vector<float> left_before = left;
    const std::vector<float> right_before = right;
    bank.process(*design, input.data(), left.data(), right.data(), K_FRAMES);

    for (std::uint32_t frame = 0; frame < K_FRAMES; ++frame)
    {
        EXPECT_EQ(left[frame], left_before[frame]) << "frame " << frame;
        EXPECT_EQ(right[frame], right_before[frame]) << "frame " << frame;
    }
}

TEST(ModalBank, ModalBankResetSilencesEveryResonator)
{
    constexpr std::uint32_t K_FRAMES = 4096;
    const auto design = single_mode_design();
    SushiDSP::room::ModalBank bank;
    bank.prepare(K_BLOCK_FRAMES);
    const StereoRender ringing = render_impulse(bank, *design, K_FRAMES);
    ASSERT_NE(ringing.left[K_FRAMES - 1], 0.0f);

    bank.reset();
    std::vector<float> silence(K_FRAMES, 0.0f);
    std::vector<float> left(K_FRAMES, 0.0f);
    std::vector<float> right(K_FRAMES, 0.0f);
    bank.process(*design, silence.data(), left.data(), right.data(), K_FRAMES);
    for (std::uint32_t frame = 0; frame < K_FRAMES; ++frame)
    {
        EXPECT_EQ(left[frame], 0.0f) << "frame " << frame;
        EXPECT_EQ(right[frame], 0.0f) << "frame " << frame;
    }
}

TEST(ModalBank, ModalBankReadsEveryRowAsItReadsTheTwoEars)
{
    // Seven modes spill into a second lane group, so a row is summed across groups too.
    auto design = std::make_unique<SushiDSP::room::ModalDesign>();
    design->count = 7u;
    std::vector<float> left_gains(SushiDSP::room::ModalDesign::K_MAX_MODES, 0.0f);
    std::vector<float> right_gains(SushiDSP::room::ModalDesign::K_MAX_MODES, 0.0f);
    for (std::uint32_t index = 0; index < design->count; ++index)
    {
        set_resonator(design->modes[index], 40.0 + 23.0 * index, 0.4 + 0.1 * index);
        design->modes[index].source_gain = 0.3f + 0.1f * static_cast<float>(index);
        design->modes[index].ear_gain = {1.0f - 0.2f * static_cast<float>(index),
                                         -0.5f + 0.15f * static_cast<float>(index)};
        left_gains[index] = design->modes[index].ear_gain[0];
        right_gains[index] = design->modes[index].ear_gain[1];
    }

    constexpr std::size_t K_FRAMES = 4 * K_BLOCK_FRAMES;
    SushiDSP::room::ModalBank two_ears;
    two_ears.prepare(K_BLOCK_FRAMES);
    const StereoRender reference = render_impulse(two_ears, *design, K_FRAMES);

    SushiDSP::room::ModalBank rows;
    rows.prepare(K_BLOCK_FRAMES);
    std::vector<float> input(K_FRAMES, 0.0f);
    input[0] = 1.0f;
    std::vector<float> first(K_FRAMES, 0.0f);
    std::vector<float> second(K_FRAMES, 0.0f);
    std::vector<float> third(K_FRAMES, 0.0f);
    for (std::size_t start = 0; start < K_FRAMES; start += K_BLOCK_FRAMES)
    {
        const SushiDSP::room::ModalBank::ReadOut read_outs[3] = {
            {left_gains.data(), first.data() + start},
            {right_gains.data(), second.data() + start},
            {left_gains.data(), third.data() + start},
        };
        rows.process(*design, input.data() + start, read_outs, 3u, K_BLOCK_FRAMES);
    }

    for (std::size_t frame = 0; frame < K_FRAMES; ++frame)
    {
        ASSERT_EQ(first[frame], reference.left[frame]) << "frame " << frame;
        ASSERT_EQ(second[frame], reference.right[frame]) << "frame " << frame;
        ASSERT_EQ(third[frame], first[frame]) << "frame " << frame;
    }
    EXPECT_GT(window_rms(first, 0, K_FRAMES), 0.0);
}
