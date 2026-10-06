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

#include "SushiDSP/math/Fft.hpp"
#include "SushiDSP/room/ModalBank.hpp"

namespace
{
    /// @brief Holds pi.
    constexpr double kPi = 3.14159265358979323846;

    /// @brief Holds the sample rate every test renders at.
    constexpr double kSampleRate = 48000.0;

    /// @brief Holds the block size the renders are split into.
    constexpr std::uint32_t kBlockFrames = 256;

    /// @brief Holds the garage's 2.5 m axial mode.
    constexpr double kGarageAxialHz = 68.6;

    /// @brief Holds the reference resonator's 60 dB decay time in seconds.
    constexpr double kDecaySeconds = 1.0;

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
        const double radius = std::pow(10.0, -3.0 / (decay_seconds * kSampleRate));
        const double theta = 2.0 * kPi * frequency_hz / kSampleRate;
        mode.section = { 1.0, 0.0, -1.0, -2.0 * radius * std::cos(theta), radius * radius };
        mode.source_gain = 1.0f;
        mode.ear_gain = { 1.0f, 1.0f };
    }

    /// @brief Returns a design holding one resonator, the garage's 68.6 Hz mode at a 1.0 s decay:
    /// r = 0.99985610, theta = 0.00897972, a1 = -1.99963157, a2 = 0.99971222.
    std::unique_ptr<SushiDSP::room::ModalDesign> single_mode_design()
    {
        auto design = std::make_unique<SushiDSP::room::ModalDesign>();
        design->count = 1u;
        set_resonator(design->modes[0], kGarageAxialHz, kDecaySeconds);
        return design;
    }

    /// @brief Renders a unit impulse through a prepared bank over frames, kBlockFrames at a
    /// time, into zeroed outputs.
    StereoRender render_impulse(SushiDSP::room::ModalBank& bank,
                                const SushiDSP::room::ModalDesign& design, std::size_t frames)
    {
        std::vector<float> input(frames, 0.0f);
        input[0] = 1.0f;
        StereoRender render{ std::vector<float>(frames, 0.0f), std::vector<float>(frames, 0.0f) };
        for (std::size_t start = 0; start < frames; start += kBlockFrames)
        {
            const auto count = static_cast<std::uint32_t>(std::min<std::size_t>(kBlockFrames,
                                                                                  frames - start));
            bank.process(design, input.data() + start, render.left.data() + start,
                         render.right.data() + start, count);
        }
        return render;
    }

    /// @brief Returns the RMS of window_frames samples starting at start.
    double window_rms(const std::vector<float>& signal, std::size_t start, std::size_t window_frames)
    {
        double sum = 0.0;
        for (std::size_t frame = start; frame < start + window_frames; ++frame)
        {
            sum += static_cast<double>(signal[frame]) * static_cast<double>(signal[frame]);
        }
        return std::sqrt(sum / static_cast<double>(window_frames));
    }
}

TEST(Unit_Dsp, ModalBankRingsAtEachModesFrequency)
{
    constexpr std::uint32_t kFftSize = 65536;
    const auto design = single_mode_design();
    SushiDSP::room::ModalBank bank;
    bank.prepare(kBlockFrames);
    const StereoRender render = render_impulse(bank, *design, kFftSize);

    SushiDSP::Fft fft(kFftSize);
    std::vector<float> real(kFftSize);
    std::vector<float> imaginary(kFftSize);
    fft.forward(render.left.data(), real.data(), imaginary.data());
    std::vector<double> power_db(kFftSize / 2 + 1, 0.0);
    std::size_t peak_bin = 1;
    for (std::size_t bin = 1; bin <= kFftSize / 2; ++bin)
    {
        const double power = static_cast<double>(real[bin]) * static_cast<double>(real[bin])
            + static_cast<double>(imaginary[bin]) * static_cast<double>(imaginary[bin]);
        power_db[bin] = 10.0 * std::log10(power);
        peak_bin = power_db[bin] > power_db[peak_bin] ? bin : peak_bin;
    }
    // A parabola through three bins places the peak. See tests/functional/unit/README.md.
    const double below = power_db[peak_bin - 1];
    const double at = power_db[peak_bin];
    const double above = power_db[peak_bin + 1];
    const double offset = 0.5 * (below - above) / (below - 2.0 * at + above);
    const double peak_hz = (static_cast<double>(peak_bin) + offset) * kSampleRate / kFftSize;
    EXPECT_NEAR(peak_hz, kGarageAxialHz, 0.005 * kGarageAxialHz);
}

TEST(Unit_Dsp, ModalBankDecaysAtTheDampingItWasGiven)
{
    // The slope between two windows is the 60 dB time. See tests/functional/unit/README.md.
    constexpr std::size_t kFrames = 96000;
    constexpr std::size_t kWindowFrames = 1024;
    constexpr std::size_t kEarlyWindow = 5;
    constexpr std::size_t kLateWindow = 40;
    const auto design = single_mode_design();
    SushiDSP::room::ModalBank bank;
    bank.prepare(kBlockFrames);
    const StereoRender render = render_impulse(bank, *design, kFrames);

    const double early_db = 20.0 * std::log10(
        window_rms(render.left, kEarlyWindow * kWindowFrames, kWindowFrames));
    const double late_db = 20.0 * std::log10(
        window_rms(render.left, kLateWindow * kWindowFrames, kWindowFrames));
    const double slope_db_per_frame = (early_db - late_db)
        / static_cast<double>((kLateWindow - kEarlyWindow) * kWindowFrames);
    const double decay_seconds = 60.0 / slope_db_per_frame / kSampleRate;
    EXPECT_NEAR(decay_seconds, kDecaySeconds, 0.05 * kDecaySeconds);
}

TEST(Unit_Dsp, ModalBankScalesEachModeByItsSourceAndEarGains)
{
    constexpr std::size_t kFrames = 4096;
    const auto unit_design = single_mode_design();
    SushiDSP::room::ModalBank unit_bank;
    unit_bank.prepare(kBlockFrames);
    const StereoRender unit = render_impulse(unit_bank, *unit_design, kFrames);

    const auto scaled_design = single_mode_design();
    scaled_design->modes[0].source_gain = 0.5f;
    scaled_design->modes[0].ear_gain = { 1.0f, -1.0f };
    SushiDSP::room::ModalBank scaled_bank;
    scaled_bank.prepare(kBlockFrames);
    const StereoRender scaled = render_impulse(scaled_bank, *scaled_design, kFrames);

    for (std::size_t frame = 0; frame < kFrames; ++frame)
    {
        EXPECT_EQ(scaled.right[frame], -scaled.left[frame]) << "frame " << frame;
        EXPECT_NEAR(scaled.left[frame], 0.5f * unit.left[frame], 1e-7f) << "frame " << frame;
    }
}

TEST(Unit_Dsp, ModalBankRunsFiveHundredAndTwelveModesWithoutDenormals)
{
    constexpr std::size_t kFrames = 96000;
    constexpr double kLowestHz = 20.0;
    constexpr double kHighestHz = 500.0;
    auto design = std::make_unique<SushiDSP::room::ModalDesign>();
    design->count = static_cast<std::uint32_t>(SushiDSP::room::ModalDesign::kMaxModes);
    for (std::size_t index = 0; index < SushiDSP::room::ModalDesign::kMaxModes; ++index)
    {
        const double fraction = static_cast<double>(index)
            / static_cast<double>(SushiDSP::room::ModalDesign::kMaxModes - 1);
        set_resonator(design->modes[index], kLowestHz + fraction * (kHighestHz - kLowestHz),
                      kDecaySeconds);
    }
    SushiDSP::room::ModalBank bank;
    bank.prepare(kBlockFrames);
    const StereoRender render = render_impulse(bank, *design, kFrames);

    std::size_t denormal_count = 0;
    for (std::size_t frame = 0; frame < kFrames; ++frame)
    {
        denormal_count += std::fpclassify(render.left[frame]) == FP_SUBNORMAL ? 1u : 0u;
        denormal_count += std::fpclassify(render.right[frame]) == FP_SUBNORMAL ? 1u : 0u;
    }
    EXPECT_EQ(denormal_count, 0u);
    EXPECT_NE(render.left[1], 0.0f);
}

TEST(Unit_Dsp, ModalBankWithNoModesAddsNothing)
{
    constexpr std::uint32_t kFrames = 512;
    auto design = std::make_unique<SushiDSP::room::ModalDesign>();
    design->count = 0u;
    SushiDSP::room::ModalBank bank;
    bank.prepare(kBlockFrames);

    std::vector<float> input(kFrames, 1.0f);
    std::vector<float> left(kFrames);
    std::vector<float> right(kFrames);
    for (std::uint32_t frame = 0; frame < kFrames; ++frame)
    {
        left[frame] = 0.25f + static_cast<float>(frame);
        right[frame] = -0.75f - static_cast<float>(frame);
    }
    const std::vector<float> left_before = left;
    const std::vector<float> right_before = right;
    bank.process(*design, input.data(), left.data(), right.data(), kFrames);

    for (std::uint32_t frame = 0; frame < kFrames; ++frame)
    {
        EXPECT_EQ(left[frame], left_before[frame]) << "frame " << frame;
        EXPECT_EQ(right[frame], right_before[frame]) << "frame " << frame;
    }
}

TEST(Unit_Dsp, ModalBankResetSilencesEveryResonator)
{
    constexpr std::uint32_t kFrames = 4096;
    const auto design = single_mode_design();
    SushiDSP::room::ModalBank bank;
    bank.prepare(kBlockFrames);
    const StereoRender ringing = render_impulse(bank, *design, kFrames);
    ASSERT_NE(ringing.left[kFrames - 1], 0.0f);

    bank.reset();
    std::vector<float> silence(kFrames, 0.0f);
    std::vector<float> left(kFrames, 0.0f);
    std::vector<float> right(kFrames, 0.0f);
    bank.process(*design, silence.data(), left.data(), right.data(), kFrames);
    for (std::uint32_t frame = 0; frame < kFrames; ++frame)
    {
        EXPECT_EQ(left[frame], 0.0f) << "frame " << frame;
        EXPECT_EQ(right[frame], 0.0f) << "frame " << frame;
    }
}
