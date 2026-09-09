/**************************************************************************/
/* test_late_reverb_design.cpp                                            */
/**************************************************************************/
/*                         This file is part of:                          */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                        https://sushisystems.io                         */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License");        */
/* you may not use this file except in compliance with the License.       */
/* You may obtain a copy of the License at                                */
/*                                                                        */
/*     http://www.apache.org/licenses/LICENSE-2.0                         */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

/**
 * @file test_late_reverb_design.cpp
 * @brief Tests for LateReverbDesign, the room-to-FDN design of the late tail.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <set>

#include "RoomFixtures.hpp"
#include "SushiDSP/room/BandResponse.hpp"
#include "SushiDSP/room/BandResponseFit.hpp"
#include "SushiDSP/room/EarlyTapSet.hpp"
#include "SushiDSP/room/EyringDecay.hpp"
#include "SushiDSP/room/FdnDesign.hpp"
#include "SushiDSP/room/ImageSourceSolver.hpp"
#include "SushiDSP/room/LateReverbDesign.hpp"
#include "SushiDSP/room/RoomScene.hpp"

namespace
{
    using SushiDSP::room::BandResponseFit;
    using SushiDSP::room::EarlyTapSet;
    using SushiDSP::room::FdnDesign;
    using SushiDSP::room::LateReverbDesign;
    using SushiDSP::room::ListenerKind;

    /// @brief Holds the sample rate every design below is written for.
    constexpr double kSampleRate = 48000.0;

    /// @brief Counts the lines.
    constexpr std::size_t kLines = FdnDesign::kLineCount;

    /// @brief Returns the garage's order-4 image set for the ears at 48 kHz.
    EarlyTapSet garage_early_taps()
    {
        return SushiDSP::room::ImageSourceSolver::solve(SushiDSP::test::garage_scene(), {});
    }

    /// @brief Returns the garage's design for the ears at 48 kHz.
    FdnDesign garage_design()
    {
        return LateReverbDesign::design(SushiDSP::test::garage_scene(), garage_early_taps(),
                                        kSampleRate, ListenerKind::Ears);
    }

    /// @brief Returns whether n is prime by trial division.
    bool is_prime(std::uint32_t n)
    {
        if (n < 2u)
        {
            return false;
        }
        for (std::uint32_t divisor = 2u; divisor * divisor <= n; ++divisor)
        {
            if (n % divisor == 0u)
            {
                return false;
            }
        }
        return true;
    }

    /// @brief Returns the sum of sixteen line lengths.
    std::uint64_t total_frames(const std::array<std::uint32_t, kLines>& lines)
    {
        std::uint64_t total = 0u;
        for (const std::uint32_t frames : lines)
        {
            total += frames;
        }
        return total;
    }

    /// @brief Returns one design section's linear magnitude at a frequency.
    double section_gain(const std::array<double, 5>& section, double frequency_hz)
    {
        const BandResponseFit::Coefficients coefficients{ section, { 1.0, 0.0, 0.0, 0.0, 0.0 } };
        return std::pow(10.0, BandResponseFit::magnitude_db(coefficients, frequency_hz, kSampleRate)
                                  / 20.0);
    }
}

TEST(Unit_Dsp, LateReverbDesignMeetsTheModalOverlapRule)
{
    // Garage: Eyring gives RT60(500 Hz) = 1.160756 s, so the sixteen lines must total at least
    // 1.4 * 1.160756 = 1.625058 s, which is 78002.8 frames at 48 kHz. The plan's 78019 rounds
    // the decay to 1.161 s first; both bounds hold.
    const FdnDesign design = garage_design();
    const std::uint64_t total = total_frames(design.line_frames);
    EXPECT_GE(total, 78003u);
    EXPECT_GE(total, 78019u);
    EXPECT_LT(total, 90000u);

    // The same rule holds for the other two rooms from their own mid-band decay.
    for (const SushiDSP::room::RoomScene& scene :
         { SushiDSP::test::bedroom_scene(), SushiDSP::test::block_room_scene() })
    {
        const double rt60_mid = SushiDSP::room::EyringDecay::rt60_seconds(scene)[2];
        const FdnDesign other = LateReverbDesign::design(
            scene, SushiDSP::room::ImageSourceSolver::solve(scene, {}), kSampleRate,
            ListenerKind::Ears);
        EXPECT_GE(static_cast<double>(total_frames(other.line_frames)),
                  1.4 * rt60_mid * kSampleRate);
    }
}

TEST(Unit_Dsp, LateReverbDesignUsesSixteenDistinctPrimes)
{
    // The garage's mean line is 1.4 * 1.160756 / 16 = 0.1015662 s, 4875.18 frames. The targets
    // 4875.18 * 2^(k / 15 - 1/2), rounded and stepped up to the next prime, land on 3449, 3613,
    // 3793, 3967, 4153, 4349, 4549, 4783, 4993, 5227, 5477, 5737, 6007, 6287, 6599 and 6899,
    // totalling 79882. The plan's seventh entry, 4561, skips the prime 4549 that the rounded
    // target 4549 already is, and its 79894 carries that 12-frame difference; the mean rounded to
    // 4876 frames gives the same sixteen. Every entry is prime, none repeats, and max/min is 2.00.
    const std::array<std::uint32_t, kLines> expected = {
        3449, 3613, 3793, 3967, 4153, 4349, 4549, 4783,
        4993, 5227, 5477, 5737, 6007, 6287, 6599, 6899
    };
    const FdnDesign design = garage_design();
    EXPECT_EQ(design.line_frames, expected);
    EXPECT_EQ(total_frames(design.line_frames), 79882u);

    std::set<std::uint32_t> distinct;
    for (const std::uint32_t frames : design.line_frames)
    {
        EXPECT_TRUE(is_prime(frames)) << frames;
        distinct.insert(frames);
    }
    EXPECT_EQ(distinct.size(), kLines);
    EXPECT_NEAR(static_cast<double>(design.line_frames[kLines - 1])
                    / static_cast<double>(design.line_frames[0]),
                2.0, 0.005);

    // line_frames alone: a 100 ms mean asks for 4800 frames, and the ends are the primes at or
    // above 4800 / sqrt(2) = 3394.1 and 4800 * sqrt(2) = 6788.2: 3407 and 6791.
    const std::array<std::uint32_t, kLines> mid = LateReverbDesign::line_frames(0.1, kSampleRate);
    EXPECT_EQ(mid[0], 3407u);
    EXPECT_EQ(mid[kLines - 1], 6791u);

    // A zero mean still yields sixteen distinct primes, the first sixteen, so the network never
    // sees a zero or a repeated length.
    const std::array<std::uint32_t, kLines> shortest = LateReverbDesign::line_frames(0.0, kSampleRate);
    const std::array<std::uint32_t, kLines> first_primes = {
        2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53
    };
    EXPECT_EQ(shortest, first_primes);
}

TEST(Unit_Dsp, LateReverbDesignFitsTheDecayEachBandAsks)
{
    // Line 0 is 3449 frames, tau_0 = 0.0718542 s. Against the garage's six decays 1.341523,
    // 1.332626, 1.160756, 1.146019, 1.108081 and 0.893129 s the wanted gains 10^(-3 tau_0 / RT60)
    // are -3.214, -3.235, -3.714, -3.762, -3.891 and -4.827 dB; the cascade lands within 1 dB.
    const SushiDSP::room::RoomScene garage = SushiDSP::test::garage_scene();
    const std::array<double, 6> rt60 = SushiDSP::room::EyringDecay::rt60_seconds(garage);
    const FdnDesign design = garage_design();
    for (std::size_t line = 0; line < kLines; ++line)
    {
        const double tau = static_cast<double>(design.line_frames[line]) / kSampleRate;
        const BandResponseFit::Coefficients coefficients{ design.line_section_a[line],
                                                          design.line_section_b[line] };
        for (std::size_t band = 0; band < 6u; ++band)
        {
            const double wanted_db = 20.0 * std::log10(std::pow(10.0, -3.0 * tau / rt60[band]));
            const double got_db = BandResponseFit::magnitude_db(
                coefficients, SushiDSP::room::kBandCentresHz[band], kSampleRate);
            EXPECT_NEAR(got_db, wanted_db, 1.0) << "line " << line << " band " << band;
        }
    }
    EXPECT_NEAR(20.0 * std::log10(std::pow(10.0, -3.0 * 3449.0 / kSampleRate / rt60[0])), -3.214,
                0.001);
}

TEST(Unit_Dsp, LateReverbDesignDelaysTheTailToTheHandoverTime)
{
    // t_c = 5 * 2.5 / 343 = 0.0364431 s, 1749.27 frames; delay_to_crossover_frames is its
    // rounding, 1749, exactly.
    const EarlyTapSet early = garage_early_taps();
    EXPECT_NEAR(early.crossover_time_s, 5.0 * 2.5 / 343.0, 1e-6);
    const FdnDesign design = garage_design();
    EXPECT_EQ(design.delay_to_crossover_frames, 1749.0f);
    EXPECT_EQ(design.delay_to_crossover_frames,
              static_cast<float>(std::round(early.crossover_time_s * kSampleRate)));
}

TEST(Unit_Dsp, LateReverbDesignScalesTheInputToContinueTheEarlyEnergy)
{
    // Halving every early tap's gain, and with it quartering the scattered energy the same set
    // would have carried, halves input_gain within 1e-6: the match is linear in pressure.
    const SushiDSP::room::RoomScene garage = SushiDSP::test::garage_scene();
    const EarlyTapSet early = garage_early_taps();
    EarlyTapSet halved = early;
    for (std::uint32_t index = 0; index < halved.count; ++index)
    {
        halved.taps[index].gain *= 0.5f;
    }
    halved.scattered_energy *= 0.25f;

    const FdnDesign full = LateReverbDesign::design(garage, early, kSampleRate, ListenerKind::Ears);
    const FdnDesign half =
        LateReverbDesign::design(garage, halved, kSampleRate, ListenerKind::Ears);
    EXPECT_GT(full.input_gain, 0.0f);
    EXPECT_NEAR(half.input_gain / full.input_gain, 0.5, 1e-6);

    // An empty set with no scattered energy asks for a silent tail.
    EarlyTapSet silent{};
    silent.count = 0;
    silent.crossover_time_s = early.crossover_time_s;
    silent.scattered_energy = 0.0f;
    const FdnDesign quiet =
        LateReverbDesign::design(garage, silent, kSampleRate, ListenerKind::Ears);
    EXPECT_EQ(quiet.input_gain, 0.0f);
}

TEST(Unit_Dsp, LateReverbDesignMakesTheTailCoherentLowAndIncoherentHigh)
{
    // sin(k d) / (k d) at d = 0.17 m is 0.975 at 125 Hz, 0.902 at 250 Hz and 0.0088 in magnitude
    // at 4 kHz; the plan's 0.900 is the 250 Hz value and its 0.0257 matches no band. The
    // coherence filter's magnitude is above 0.85 at 125 Hz and below 0.20 at 4 kHz, and the
    // pair is power complementary at every centre within 1e-6.
    const FdnDesign design = garage_design();
    EXPECT_GT(section_gain(design.coherence_a, 125.0), 0.85);
    EXPECT_LT(section_gain(design.coherence_a, 4000.0), 0.20);
    EXPECT_LT(section_gain(design.coherence_b, 125.0), 0.20);
    EXPECT_GT(section_gain(design.coherence_b, 4000.0), 0.95);
    for (const double frequency_hz : SushiDSP::room::kBandCentresHz)
    {
        const double a = section_gain(design.coherence_a, frequency_hz);
        const double b = section_gain(design.coherence_b, frequency_hz);
        EXPECT_NEAR(a * a + b * b, 1.0, 1e-6) << frequency_hz << " Hz";
    }

    // The tone correction is scaled to unity at 500 Hz, the band the mean line came from.
    EXPECT_NEAR(section_gain(design.tone_correction, 500.0), 1.0, 0.02);
}

TEST(Unit_Dsp, LateReverbDesignGivesTheMicrophoneOneOutput)
{
    // ListenerKind::Microphone leaves both coherence filters as pass-throughs: b0 = 1, the rest
    // 0. Everything else in the design is the listener's independent.
    const SushiDSP::room::RoomScene garage = SushiDSP::test::garage_scene();
    const SushiDSP::room::ImageSourceSolver::Options options{ 4u, ListenerKind::Microphone,
                                                              kSampleRate };
    const EarlyTapSet early = SushiDSP::room::ImageSourceSolver::solve(garage, options);
    const FdnDesign design =
        LateReverbDesign::design(garage, early, kSampleRate, ListenerKind::Microphone);
    const std::array<double, 5> unity = { 1.0, 0.0, 0.0, 0.0, 0.0 };
    EXPECT_EQ(design.coherence_a, unity);
    EXPECT_EQ(design.coherence_b, unity);
    EXPECT_EQ(design.line_frames, garage_design().line_frames);
}
