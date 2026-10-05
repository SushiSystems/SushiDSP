/****************************************************************/
/* test_late_reverb_design.cpp                                  */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

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
#include <vector>

#include "RoomFixtures.hpp"
#include "SushiDSP/math/Fft.hpp"
#include "SushiDSP/room/BandResponse.hpp"
#include "SushiDSP/room/BandResponseFit.hpp"
#include "SushiDSP/room/DiffuseField.hpp"
#include "SushiDSP/room/EarlyTapSet.hpp"
#include "SushiDSP/room/EyringDecay.hpp"
#include "SushiDSP/room/FdnDesign.hpp"
#include "SushiDSP/room/LateReverb.hpp"
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

    /// @brief Returns one design cascade's magnitude in decibels at a frequency.
    double cascade_db(const FdnDesign::Cascade& cascade, double frequency_hz)
    {
        const BandResponseFit::Coefficients coefficients{ cascade[0], cascade[1] };
        return BandResponseFit::magnitude_db(coefficients, frequency_hz, kSampleRate);
    }

    /// @brief Returns one design cascade's linear magnitude at a frequency.
    double cascade_gain(const FdnDesign::Cascade& cascade, double frequency_hz)
    {
        return std::pow(10.0, cascade_db(cascade, frequency_hz) / 20.0);
    }

    /// @brief Holds six band figures.
    using Bands = std::array<double, 6>;

    /// @brief Returns 10 log10 of a power, floored far under any figure here.
    double decibels(double power)
    {
        return 10.0 * std::log10(std::max(power, 1e-30));
    }

    /// @brief Returns the energy the taps past the direct sound carry at each band centre:
    /// gain squared times the fitted filter's power.
    Bands reflected_energy(const EarlyTapSet& early)
    {
        Bands energy{};
        for (std::uint32_t index = 1; index < early.count; ++index)
        {
            const BandResponseFit::Coefficients coefficients{ early.taps[index].section_a,
                                                              early.taps[index].section_b };
            const double gain = static_cast<double>(early.taps[index].gain);
            for (std::size_t band = 0; band < 6u; ++band)
            {
                const double db = BandResponseFit::magnitude_db(
                    coefficients, SushiDSP::room::kBandCentresHz[band], kSampleRate);
                energy[band] += gain * gain * std::pow(10.0, db / 10.0);
            }
        }
        return energy;
    }

    /// @brief Returns what the diffuse field holds beyond the reflections per band, floored at
    /// zero.
    Bands remainder_of(const SushiDSP::room::RoomScene& scene, const EarlyTapSet& early)
    {
        const SushiDSP::room::DiffuseField field(scene);
        const Bands reflected = reflected_energy(early);
        Bands remainder{};
        for (std::size_t band = 0; band < 6u; ++band)
        {
            remainder[band] = std::max(field.band_power()[band] - reflected[band], 0.0);
        }
        return remainder;
    }

    /// @brief Returns the energy sixteen lines deliver per band for a unit input gain:
    /// 16 G / (1 - G) with G the mean over lines of 10^(-6 tau_i / RT60(f)).
    Bands network_energy(const SushiDSP::room::RoomScene& scene, const FdnDesign& design)
    {
        const Bands rt60 = SushiDSP::room::EyringDecay::rt60_seconds(scene);
        Bands energy{};
        for (std::size_t band = 0; band < 6u; ++band)
        {
            double kept = 0.0;
            for (const std::uint32_t frames : design.line_frames)
            {
                kept += std::pow(10.0, -6.0 * (frames / kSampleRate) / rt60[band]) / 16.0;
            }
            energy[band] = 16.0 * kept / (1.0 - kept);
        }
        return energy;
    }

    /// @brief Returns the tail's rendered energy per band: an impulse through LateReverb at
    /// the design, the power spectrum's mean over each octave.
    Bands rendered_energy(const FdnDesign& design)
    {
        constexpr std::uint32_t kFrames = 131072;
        SushiDSP::room::LateReverb late;
        late.prepare(25200);
        std::vector<float> input(kFrames, 0.0f);
        input[0] = 1.0f;
        std::vector<float> output(kFrames, 0.0f);
        for (std::uint32_t offset = 0; offset < kFrames; offset += 256u)
        {
            late.process(design, input.data() + offset, output.data() + offset,
                         output.data() + offset, 256u);
        }
        SushiDSP::Fft fft(kFrames);
        std::vector<float> real(kFrames);
        std::vector<float> imaginary(kFrames);
        fft.forward(output.data(), real.data(), imaginary.data());
        const double bin_hz = kSampleRate / kFrames;
        Bands energy{};
        for (std::size_t band = 0; band < 6u; ++band)
        {
            const double centre = SushiDSP::room::kBandCentresHz[band];
            const auto first =
                static_cast<std::size_t>(std::ceil(centre / std::sqrt(2.0) / bin_hz));
            const auto last =
                static_cast<std::size_t>(std::ceil(centre * std::sqrt(2.0) / bin_hz));
            double sum = 0.0;
            for (std::size_t bin = first; bin < last; ++bin)
            {
                sum += static_cast<double>(real[bin]) * real[bin]
                    + static_cast<double>(imaginary[bin]) * imaginary[bin];
            }
            energy[band] = sum / static_cast<double>(last - first);
        }
        return energy;
    }
}

TEST(Unit_Dsp, LateReverbDesignMeetsTheModalOverlapRule)
{
    // The lines total at least 1.4 * RT60(500 Hz). See tests/functional/unit/README.md.
    const FdnDesign design = garage_design();
    const std::uint64_t total = total_frames(design.line_frames);
    EXPECT_GE(total, 78003u);
    EXPECT_GE(total, 78019u);
    EXPECT_LT(total, 90000u);
    // The shortest line is the handover time. See tests/functional/unit/README.md.
    EXPECT_NEAR(static_cast<double>(design.line_frames[0]), 1749.0, 8.0);

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
    // The garage's primes run from 1753 to 10211. See tests/functional/unit/README.md.
    const FdnDesign design = garage_design();
    EXPECT_EQ(design.line_frames[0], 1753u);
    EXPECT_EQ(design.line_frames[kLines - 1], 10211u);
    EXPECT_GE(total_frames(design.line_frames), 78003u);

    std::set<std::uint32_t> distinct;
    for (const std::uint32_t frames : design.line_frames)
    {
        EXPECT_TRUE(is_prime(frames)) << frames;
        distinct.insert(frames);
    }
    EXPECT_EQ(distinct.size(), kLines);

    // No line outruns the storage the renderer is sized for.
    EXPECT_LE(static_cast<double>(design.line_frames[kLines - 1]),
              FdnDesign::kMaxLineSeconds * kSampleRate);

    // The targets 4800 and 19200 step up to 4801 and 19207. See tests/functional/unit/README.md.
    const std::array<std::uint32_t, kLines> mid =
        LateReverbDesign::line_frames(0.1, 4.0, kSampleRate);
    EXPECT_EQ(mid[0], 4801u);
    EXPECT_EQ(mid[kLines - 1], 19207u);

    // A zero shortest line yields the first sixteen primes. See tests/functional/unit/README.md.
    const std::array<std::uint32_t, kLines> shortest =
        LateReverbDesign::line_frames(0.0, 2.0, kSampleRate);
    const std::array<std::uint32_t, kLines> first_primes = {
        2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53
    };
    EXPECT_EQ(shortest, first_primes);
}

TEST(Unit_Dsp, LateReverbDesignFitsTheDecayEachBandAsks)
{
    // The cascade lands within 1 dB of each wanted gain. See tests/functional/unit/README.md.
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
    EXPECT_NEAR(20.0 * std::log10(std::pow(10.0, -3.0 * 1753.0 / kSampleRate / rt60[0])), -1.634,
                0.001);
}

TEST(Unit_Dsp, LateReverbDesignDelaysTheTailToTheHandoverTime)
{
    // The delay is the handover time less the shortest line. See tests/functional/unit/README.md.
    const EarlyTapSet early = garage_early_taps();
    EXPECT_NEAR(early.crossover_time_s, 5.0 * 2.5 / 343.0, 1e-6);
    const FdnDesign design = garage_design();
    EXPECT_EQ(design.delay_to_crossover_frames, 0.0f);
    EXPECT_EQ(design.delay_to_crossover_frames,
              static_cast<float>(std::max(std::round(early.crossover_time_s * kSampleRate)
                                              - static_cast<double>(design.line_frames[0]),
                                          0.0)));

    // Every reference room hands over within a frame of t_c. See tests/functional/unit/README.md.
    for (const SushiDSP::room::RoomScene& scene :
         { SushiDSP::test::garage_scene(), SushiDSP::test::bedroom_scene(),
           SushiDSP::test::block_room_scene() })
    {
        const EarlyTapSet taps = SushiDSP::room::ImageSourceSolver::solve(scene, {});
        const FdnDesign other =
            LateReverbDesign::design(scene, taps, kSampleRate, ListenerKind::Ears);
        const double onset = static_cast<double>(other.delay_to_crossover_frames)
            + static_cast<double>(other.line_frames[0]);
        EXPECT_NEAR(onset, taps.crossover_time_s * kSampleRate, 8.0);
    }
}

TEST(Unit_Dsp, LateReverbDesignPutsEachBandsRemainderIntoTheTail)
{
    // Each band's share is floored at -30 dB of the largest. See tests/functional/unit/README.md.
    for (const SushiDSP::room::RoomScene& scene :
         { SushiDSP::test::garage_scene(), SushiDSP::test::bedroom_scene() })
    {
        const EarlyTapSet early = SushiDSP::room::ImageSourceSolver::solve(scene, {});
        const FdnDesign design =
            LateReverbDesign::design(scene, early, kSampleRate, ListenerKind::Ears);
        const Bands remainder = remainder_of(scene, early);
        const Bands network = network_energy(scene, design);
        Bands share{};
        double largest = 0.0;
        for (std::size_t band = 0; band < 6u; ++band)
        {
            share[band] = remainder[band] / network[band];
            largest = std::max(largest, share[band]);
        }
        double mean = 0.0;
        for (std::size_t band = 0; band < 6u; ++band)
        {
            share[band] = std::max(share[band], 1e-3 * largest);
            mean += share[band] / 6.0;
        }
        EXPECT_NEAR(static_cast<double>(design.input_gain), std::sqrt(mean),
                    1e-6 * std::sqrt(mean));
        for (std::size_t band = 0; band < 6u; ++band)
        {
            EXPECT_NEAR(cascade_db(design.tone_correction, SushiDSP::room::kBandCentresHz[band]),
                        decibels(share[band] / mean), 0.7)
                << SushiDSP::room::kBandCentresHz[band] << " Hz";
        }
    }

    // The bedroom's 4 kHz band sits on the floor and the garage's does not.
    const SushiDSP::room::RoomScene bedroom = SushiDSP::test::bedroom_scene();
    const EarlyTapSet bedroom_early = SushiDSP::room::ImageSourceSolver::solve(bedroom, {});
    EXPECT_EQ(remainder_of(bedroom, bedroom_early)[5], 0.0);
    EXPECT_GT(remainder_of(SushiDSP::test::garage_scene(), garage_early_taps())[5], 0.0);

    // The tail's gain grows as the taps shrink. See tests/functional/unit/README.md.
    const SushiDSP::room::RoomScene garage = SushiDSP::test::garage_scene();
    const EarlyTapSet early = garage_early_taps();
    EarlyTapSet halved = early;
    for (std::uint32_t index = 0; index < halved.count; ++index)
    {
        halved.taps[index].gain *= 0.5f;
    }
    EarlyTapSet empty{};
    empty.count = 0;
    empty.crossover_time_s = early.crossover_time_s;
    const FdnDesign full = garage_design();
    const FdnDesign half =
        LateReverbDesign::design(garage, halved, kSampleRate, ListenerKind::Ears);
    const FdnDesign alone =
        LateReverbDesign::design(garage, empty, kSampleRate, ListenerKind::Ears);
    EXPECT_GT(full.input_gain, 0.0f);
    EXPECT_GT(half.input_gain, full.input_gain);
    EXPECT_GT(alone.input_gain, half.input_gain);
}

TEST(Unit_Dsp, LateReverbDesignRendersEachBandsRemainder)
{
    // Each band renders within 1 dB, or 2 dB at 125 Hz. See tests/functional/unit/README.md.
    for (const SushiDSP::room::RoomScene& scene :
         { SushiDSP::test::garage_scene(), SushiDSP::test::bedroom_scene(),
           SushiDSP::test::block_room_scene() })
    {
        const EarlyTapSet early = SushiDSP::room::ImageSourceSolver::solve(scene, {});
        const FdnDesign design =
            LateReverbDesign::design(scene, early, kSampleRate, ListenerKind::Ears);
        const Bands remainder = remainder_of(scene, early);
        const Bands reflected = reflected_energy(early);
        const Bands rendered = rendered_energy(design);
        for (std::size_t band = 0; band < 6u; ++band)
        {
            const double centre = SushiDSP::room::kBandCentresHz[band];
            if (remainder[band] == 0.0)
            {
                EXPECT_LT(decibels(rendered[band]), decibels(reflected[band]) - 10.0)
                    << centre << " Hz";
                continue;
            }
            const double error_db = decibels(rendered[band]) - decibels(remainder[band]);
            RecordProperty("band_" + std::to_string(static_cast<int>(centre)) + "_db", error_db);
            EXPECT_NEAR(error_db, 0.0, band == 0 ? 2.0 : 1.0) << centre << " Hz";
        }
    }
}

TEST(Unit_Dsp, LateReverbDesignMakesTheTailCoherentLowAndIncoherentHigh)
{
    // Coherence is above 0.85 at 125 Hz, below 0.20 at 4 kHz. See tests/functional/unit/README.md.
    const FdnDesign design = garage_design();
    EXPECT_GT(cascade_gain(design.coherence_a, 125.0), 0.85);
    EXPECT_LT(cascade_gain(design.coherence_a, 4000.0), 0.20);
    EXPECT_LT(cascade_gain(design.coherence_b, 125.0), 0.20);
    EXPECT_GT(cascade_gain(design.coherence_b, 4000.0), 0.95);
}

TEST(Unit_Dsp, LateReverbDesignKeepsTheCoherencePairPowerComplementary)
{
    // |A|^2 + |B|^2 = 1 within 1e-9. See tests/functional/unit/README.md.
    const FdnDesign design = garage_design();
    for (const double frequency_hz : { 125.0, 250.0, 446.9, 500.0, 1000.0, 2000.0, 4000.0 })
    {
        const double a = cascade_gain(design.coherence_a, frequency_hz);
        const double b = cascade_gain(design.coherence_b, frequency_hz);
        EXPECT_NEAR(a * a + b * b, 1.0, 1e-9) << frequency_hz << " Hz";
    }
}

TEST(Unit_Dsp, LateReverbDesignShapesTheOutputWithBothSections)
{
    // The second section carries part of the fall to 4 kHz. See tests/functional/unit/README.md.
    const FdnDesign design = garage_design();
    EXPECT_GT(cascade_db(design.tone_correction, 125.0), 3.5);
    EXPECT_LT(cascade_db(design.tone_correction, 4000.0), -11.0);
    const FdnDesign::Cascade first_only = { design.tone_correction[0],
                                            { 1.0, 0.0, 0.0, 0.0, 0.0 } };
    EXPECT_GT(cascade_db(first_only, 4000.0), cascade_db(design.tone_correction, 4000.0) + 1.0);
    EXPECT_NEAR(cascade_gain(design.tone_correction, 125.0)
                    / cascade_gain(design.tone_correction, 4000.0),
                std::pow(10.0, (4.7 + 12.1) / 20.0), 0.6);
}

TEST(Unit_Dsp, LateReverbDesignGivesTheMicrophoneOneOutput)
{
    // A microphone's coherence filters are pass-throughs. See tests/functional/unit/README.md.
    const SushiDSP::room::RoomScene garage = SushiDSP::test::garage_scene();
    const SushiDSP::room::ImageSourceSolver::Options options{ 4u, ListenerKind::Microphone,
                                                              kSampleRate };
    const EarlyTapSet early = SushiDSP::room::ImageSourceSolver::solve(garage, options);
    const FdnDesign design =
        LateReverbDesign::design(garage, early, kSampleRate, ListenerKind::Microphone);
    const FdnDesign::Section unity = { 1.0, 0.0, 0.0, 0.0, 0.0 };
    const FdnDesign::Cascade unity_cascade = { unity, unity };
    EXPECT_EQ(design.coherence_a, unity_cascade);
    EXPECT_EQ(design.coherence_b, unity_cascade);
    EXPECT_EQ(design.line_frames, garage_design().line_frames);
}

TEST(Unit_Dsp, LateReverbDesignHandsAMicrophoneItsShareOfTheField)
{
    // A microphone picks up a^2 + (1 - a)^2 / 3 of the field. See tests/functional/unit/README.md.
    SushiDSP::room::RoomScene omni = SushiDSP::test::garage_scene();
    omni.microphone_position = omni.listener_position;
    omni.microphone_pattern = SushiDSP::room::PolarPattern::Omni;
    SushiDSP::room::RoomScene cardioid = omni;
    cardioid.microphone_pattern = SushiDSP::room::PolarPattern::Cardioid;
    const SushiDSP::room::ImageSourceSolver::Options options{ 4u, ListenerKind::Microphone,
                                                              kSampleRate };
    const EarlyTapSet omni_early = SushiDSP::room::ImageSourceSolver::solve(omni, options);
    const EarlyTapSet cardioid_early =
        SushiDSP::room::ImageSourceSolver::solve(cardioid, options);
    const FdnDesign omni_design =
        LateReverbDesign::design(omni, omni_early, kSampleRate, ListenerKind::Microphone);
    const FdnDesign cardioid_design = LateReverbDesign::design(
        cardioid, cardioid_early, kSampleRate, ListenerKind::Microphone);

    // The omni at the listener's spot reads what the ears' set does: the same remainder.
    const FdnDesign ears = garage_design();
    EXPECT_NEAR(static_cast<double>(omni_design.input_gain), static_cast<double>(ears.input_gain),
                0.02 * ears.input_gain);

    // The design's gain follows that arithmetic within 1e-6. See tests/functional/unit/README.md.
    const SushiDSP::room::DiffuseField field(cardioid);
    const Bands network = network_energy(cardioid, cardioid_design);
    const Bands reflected = reflected_energy(cardioid_early);
    Bands share{};
    double largest = 0.0;
    for (std::size_t band = 0; band < 6u; ++band)
    {
        share[band] = std::max(field.band_power()[band] / 3.0 - reflected[band], 0.0)
            / network[band];
        largest = std::max(largest, share[band]);
    }
    double mean = 0.0;
    for (std::size_t band = 0; band < 6u; ++band)
    {
        mean += std::max(share[band], 1e-3 * largest) / 6.0;
    }
    EXPECT_NEAR(static_cast<double>(cardioid_design.input_gain), std::sqrt(mean),
                1e-6 * std::sqrt(mean));
    EXPECT_LT(cardioid_design.input_gain, omni_design.input_gain);
}
