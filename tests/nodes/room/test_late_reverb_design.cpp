/**************************************************************************/
/* test_late_reverb_design.cpp                                            */
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
 * @file test_late_reverb_design.cpp
 * @brief Tests for LateReverbDesign, the room-to-FDN design of the late tail.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <SushiDSP/nodes/room_scenes/room_fixtures.hpp>
#include <SushiDSP/foundation/math/fft.hpp>
#include <SushiDSP/nodes/room/band_response.hpp>
#include <SushiDSP/nodes/room/band_response_fit.hpp>
#include <SushiDSP/nodes/room/diffuse_field.hpp>
#include <SushiDSP/nodes/room/early_tap_set.hpp>
#include <SushiDSP/nodes/room/eyring_decay.hpp>
#include <SushiDSP/nodes/room/fdn_design.hpp>
#include <SushiDSP/nodes/room/late_reverb.hpp>
#include <SushiDSP/nodes/room/image_source_solver.hpp>
#include <SushiDSP/nodes/room/late_reverb_design.hpp>
#include <SushiDSP/nodes/room/room_scene.hpp>

#include "support/tolerance.hpp"

namespace
{
    using SushiDSP::room::BandResponseFit;
    using SushiDSP::room::EarlyTapSet;
    using SushiDSP::room::FdnDesign;
    using SushiDSP::room::LateReverbDesign;
    using SushiDSP::room::ListenerKind;
    using SushiDSP::room::RoomScene;

    /// @brief Holds the sample rate every design below is written for.
    constexpr double K_SAMPLE_RATE = 48000.0;

    /// @brief Counts the lines.
    constexpr std::size_t K_LINES = FdnDesign::K_LINE_COUNT;

    /// @brief Counts the bands.
    constexpr std::size_t K_BANDS = 6;

    /// @brief Holds the garage's 500 Hz decay times 1.4, rounded up the way the plan rounds
    /// it: 1.4 * 1.161 s * 48000 = 78019 frames. The unrounded figure is 78002.8.
    constexpr std::uint64_t K_GARAGE_OVERLAP_FRAMES = 78019;

    /// @brief Holds the frames the garage's lines may total at most. Characterised, not derived:
    /// it is the figure under which the first prime above each target keeps the sum.
    constexpr std::uint64_t K_GARAGE_TOTAL_CEILING_FRAMES = 90000;

    /// @brief Holds the garage's shortest line: the first prime at or above its handover time of
    /// 1749.27 frames, since 1749 = 3 * 11 * 53 and 1751 = 17 * 103.
    constexpr std::uint32_t K_GARAGE_SHORTEST_LINE = 1753;

    /// @brief Holds the garage's longest line: the first prime at or above its target.
    constexpr std::uint32_t K_GARAGE_LONGEST_LINE = 10211;

    /// @brief Holds the decibels the line fits stay within at every band centre, which
    /// test_band_response_fit.cpp holds the fit to for every material and order.
    constexpr double K_FIT_CLEARANCE_DB = 1.0;

    /// @brief Holds the decibels the output cascade may sit from the band shares it fits.
    /// Characterised, not derived: a two-section fit follows a 30 dB range of targets.
    constexpr double K_TONE_FIT_DB = 0.7;

    /// @brief Bounds a band's share of the tail from below at -30 dB of the largest band's, the
    /// floor the design applies.
    constexpr double K_SHARE_FLOOR = 1e-3;

    /// @brief Holds how far the power sum of the coherence pair may sit from 1. Characterised, not
    /// derived: the pair is exact in real arithmetic, and the rounding of its coefficients is
    /// amplified by the poles' closeness to the unit circle at the corner near 447 Hz.
    constexpr double K_COMPLEMENT_TOLERANCE = 1e-9;

    /// @brief Holds the decibels a band's rendered energy may sit from its remainder above 125 Hz.
    /// Characterised, not derived: the fits are anchored on both sides of those bands.
    constexpr double K_RENDER_BAND_DB = 1.0;

    /// @brief Holds the decibels the 125 Hz band may sit off, whose octave runs down to 88 Hz where
    /// the line filters and the output cascade hold whatever their low shelves settle to.
    /// Characterised, not derived.
    constexpr double K_RENDER_LOW_BAND_DB = 2.0;

    /// @brief Holds the decibels a band with no remainder must render under the reflections it
    /// would sit beneath. Characterised, not derived.
    constexpr double K_EMPTY_BAND_MARGIN_DB = 10.0;

    /// @brief Holds the relative distance an omni microphone at the listener's spot may have from
    /// the ears' input gain. Characterised, not derived: the two sets differ in their receiver.
    constexpr double K_OMNI_VERSUS_EARS_TOLERANCE = 0.02;

    /// @brief Holds the figure the garage's output cascade falls by between 125 Hz and 4 kHz, in
    /// decibels, as measured: from +4.7 dB to -12.1 dB.
    constexpr double K_GARAGE_TONE_FALL_DB = 4.7 + 12.1;

    /// @brief Holds the distance the garage's measured tone ratio may sit from the measured fall.
    /// Characterised, not derived.
    constexpr double K_GARAGE_TONE_RATIO_TOLERANCE = 0.6;

    /// @brief Holds six band figures.
    using Bands = std::array<double, K_BANDS>;

    /// @brief Returns the scene of one reference room.
    using SceneFactory = RoomScene (*)();

    /// @brief Holds one reference room and the name its test carries.
    struct Room
    {
            const char* name;
            SceneFactory scene;
    };

    /// @brief Holds the three reference rooms.
    const std::array<Room, 3> kAllRooms = {{{"Garage", &SushiDSP::test::garage_scene},
                                            {"Bedroom", &SushiDSP::test::bedroom_scene},
                                            {"BlockRoom", &SushiDSP::test::block_room_scene}}};

    /// @brief Holds the two rooms whose tail has a remainder to put into the network.
    const std::array<Room, 2> kTailRooms = {{{"Garage", &SushiDSP::test::garage_scene},
                                             {"Bedroom", &SushiDSP::test::bedroom_scene}}};

    /// @brief Names a room case after its room.
    std::string room_name(const ::testing::TestParamInfo<Room>& info)
    {
        return info.param.name;
    }

    /// @brief Holds one coherence frequency and the name its test carries.
    struct Frequency
    {
            const char* name;
            double hertz;
    };

    /// @brief Holds the frequencies the coherence pair is checked at: the band centres and the
    /// corner between them.
    const std::array<Frequency, 7> kFrequencies = {{{"Hz125", 125.0},
                                                    {"Hz250", 250.0},
                                                    {"Hz446", 446.9},
                                                    {"Hz500", 500.0},
                                                    {"Hz1000", 1000.0},
                                                    {"Hz2000", 2000.0},
                                                    {"Hz4000", 4000.0}}};

    /// @brief Names a frequency case after its frequency.
    std::string frequency_name(const ::testing::TestParamInfo<Frequency>& info)
    {
        return info.param.name;
    }

    /// @brief Returns the garage's order-4 image set for the ears at 48 kHz.
    EarlyTapSet garage_early_taps()
    {
        return SushiDSP::room::ImageSourceSolver::solve(SushiDSP::test::garage_scene(), {});
    }

    /// @brief Returns the garage's design for the ears at 48 kHz.
    FdnDesign garage_design()
    {
        return LateReverbDesign::design(SushiDSP::test::garage_scene(), garage_early_taps(),
                                        K_SAMPLE_RATE, ListenerKind::Ears);
    }

    /// @brief Returns the garage's design for its default microphone.
    FdnDesign garage_microphone_design()
    {
        const RoomScene garage = SushiDSP::test::garage_scene();
        const SushiDSP::room::ImageSourceSolver::Options options{4u, ListenerKind::Microphone,
                                                                 K_SAMPLE_RATE};
        return LateReverbDesign::design(garage,
                                        SushiDSP::room::ImageSourceSolver::solve(garage, options),
                                        K_SAMPLE_RATE, ListenerKind::Microphone);
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

    /// @brief Returns the smallest prime not below n.
    std::uint32_t next_prime_at_or_above(std::uint32_t n)
    {
        std::uint32_t candidate = n;
        while (!is_prime(candidate))
        {
            ++candidate;
        }
        return candidate;
    }

    /// @brief Returns the sum of sixteen line lengths.
    std::uint64_t total_frames(const std::array<std::uint32_t, K_LINES>& lines)
    {
        std::uint64_t total = 0u;
        for (const std::uint32_t frames : lines)
        {
            total += frames;
        }
        return total;
    }

    /// @brief Returns the frames of the handover time, rounded as the design rounds them.
    double handover_frames(const EarlyTapSet& early)
    {
        return std::round(static_cast<double>(early.crossover_time_s) * K_SAMPLE_RATE);
    }

    /// @brief Returns one design cascade's magnitude in decibels at a frequency.
    double cascade_db(const FdnDesign::Cascade& cascade, double frequency_hz)
    {
        const BandResponseFit::Coefficients coefficients{cascade[0], cascade[1]};
        return BandResponseFit::magnitude_db(coefficients, frequency_hz, K_SAMPLE_RATE);
    }

    /// @brief Returns one design cascade's linear magnitude at a frequency.
    double cascade_gain(const FdnDesign::Cascade& cascade, double frequency_hz)
    {
        return std::pow(10.0, cascade_db(cascade, frequency_hz) / 20.0);
    }

    /// @brief Returns 10 log10 of a power, floored far under any figure here.
    double decibels(double power)
    {
        return 10.0 * std::log10(std::max(power, 1e-30));
    }

    /// @brief Returns how far the worst line filter sits from the gain its line must keep per
    /// band, in decibels: 10^(-3 tau / RT60) is -60 tau / RT60 dB.
    double worst_fit_miss_db(const FdnDesign& design, const Bands& rt60)
    {
        double worst = 0.0;
        for (std::size_t line = 0; line < K_LINES; ++line)
        {
            const double tau = static_cast<double>(design.line_frames[line]) / K_SAMPLE_RATE;
            const BandResponseFit::Coefficients coefficients{design.line_section_a[line],
                                                             design.line_section_b[line]};
            for (std::size_t band = 0; band < K_BANDS; ++band)
            {
                const double wanted_db = -60.0 * tau / rt60[band];
                const double got_db = BandResponseFit::magnitude_db(
                    coefficients, SushiDSP::room::K_BAND_CENTRES_HZ[band], K_SAMPLE_RATE);
                worst = std::max(worst, std::fabs(got_db - wanted_db));
            }
        }
        return worst;
    }

    /// @brief Returns the energy the taps past the direct sound carry at each band centre:
    /// gain squared times the fitted filter's power.
    Bands reflected_energy(const EarlyTapSet& early)
    {
        Bands energy{};
        for (std::uint32_t index = early.direct_count; index < early.count; ++index)
        {
            const BandResponseFit::Coefficients coefficients{early.taps[index].section_a,
                                                             early.taps[index].section_b};
            const double gain = static_cast<double>(early.taps[index].gain);
            for (std::size_t band = 0; band < K_BANDS; ++band)
            {
                const double db = BandResponseFit::magnitude_db(
                    coefficients, SushiDSP::room::K_BAND_CENTRES_HZ[band], K_SAMPLE_RATE);
                energy[band] += gain * gain * std::pow(10.0, db / 10.0);
            }
        }
        return energy;
    }

    /// @brief Returns what a share of the diffuse field holds beyond the reflections per band,
    /// floored at zero.
    Bands remainder_of(const RoomScene& scene, const EarlyTapSet& early, double share_of_field)
    {
        const SushiDSP::room::DiffuseField field(scene);
        const Bands reflected = reflected_energy(early);
        Bands remainder{};
        for (std::size_t band = 0; band < K_BANDS; ++band)
        {
            remainder[band] =
                std::max(field.band_power()[band] * share_of_field - reflected[band], 0.0);
        }
        return remainder;
    }

    /// @brief Returns the energy sixteen lines deliver per band for a unit input gain:
    /// 16 G / (1 - G) with G the mean over lines of 10^(-6 tau_i / RT60(f)).
    Bands network_energy(const RoomScene& scene, const FdnDesign& design)
    {
        const Bands rt60 = SushiDSP::room::EyringDecay::rt60_seconds(scene);
        Bands energy{};
        for (std::size_t band = 0; band < K_BANDS; ++band)
        {
            double kept = 0.0;
            for (const std::uint32_t frames : design.line_frames)
            {
                kept += std::pow(10.0, -6.0 * (frames / K_SAMPLE_RATE) / rt60[band]) / 16.0;
            }
            energy[band] = 16.0 * kept / (1.0 - kept);
        }
        return energy;
    }

    /// @brief Holds each band's share of the tail and the mean of the shares.
    struct TailShare
    {
            /// @brief Stores the remainder over the network energy per band, floored.
            Bands share;

            /// @brief Stores the mean of the shares over the bands.
            double mean;
    };

    /// @brief Returns the shares of the tail per band, each floored at K_SHARE_FLOOR of the
    /// largest, and their mean.
    TailShare tail_share(const Bands& remainder, const Bands& network)
    {
        TailShare out{};
        double largest = 0.0;
        for (std::size_t band = 0; band < K_BANDS; ++band)
        {
            out.share[band] = remainder[band] / network[band];
            largest = std::max(largest, out.share[band]);
        }
        for (std::size_t band = 0; band < K_BANDS; ++band)
        {
            out.share[band] = std::max(out.share[band], K_SHARE_FLOOR * largest);
            out.mean += out.share[band] / static_cast<double>(K_BANDS);
        }
        return out;
    }

    /// @brief Returns how far the output cascade sits from the share of each band over the mean,
    /// at its worst band, in decibels.
    double worst_tone_miss_db(const FdnDesign& design, const TailShare& tail)
    {
        double worst = 0.0;
        for (std::size_t band = 0; band < K_BANDS; ++band)
        {
            const double got_db =
                cascade_db(design.tone_correction, SushiDSP::room::K_BAND_CENTRES_HZ[band]);
            worst = std::max(worst,
                             std::fabs(got_db - decibels(tail.share[band] / tail.mean)));
        }
        return worst;
    }

    /// @brief Returns the input gain of the garage's design with every tap scaled.
    float garage_gain_with_taps_scaled(float scale)
    {
        EarlyTapSet taps = garage_early_taps();
        for (std::uint32_t index = 0; index < taps.count; ++index)
        {
            taps.taps[index].gain *= scale;
        }
        return LateReverbDesign::design(SushiDSP::test::garage_scene(), taps, K_SAMPLE_RATE,
                                        ListenerKind::Ears)
            .input_gain;
    }

    /// @brief Returns the input gain of the garage's design with no tap at all.
    float garage_gain_with_no_taps()
    {
        const EarlyTapSet early = garage_early_taps();
        EarlyTapSet empty{};
        empty.count = 0;
        empty.crossover_time_s = early.crossover_time_s;
        return LateReverbDesign::design(SushiDSP::test::garage_scene(), empty, K_SAMPLE_RATE,
                                        ListenerKind::Ears)
            .input_gain;
    }

    /// @brief Returns the tail's rendered energy per band: an impulse through LateReverb at
    /// the design, the power spectrum's mean over each octave.
    Bands rendered_energy(const FdnDesign& design)
    {
        constexpr std::uint32_t K_FRAMES = 131072;
        SushiDSP::room::LateReverb late;
        late.prepare(25200);
        std::vector<float> input(K_FRAMES, 0.0f);
        input[0] = 1.0f;
        std::vector<float> output(K_FRAMES, 0.0f);
        for (std::uint32_t offset = 0; offset < K_FRAMES; offset += 256u)
        {
            late.process(design, input.data() + offset, output.data() + offset,
                         output.data() + offset, 256u);
        }
        SushiDSP::Fft fft(K_FRAMES);
        std::vector<float> real(K_FRAMES);
        std::vector<float> imaginary(K_FRAMES);
        fft.forward(output.data(), real.data(), imaginary.data());
        const double bin_hz = K_SAMPLE_RATE / K_FRAMES;
        Bands energy{};
        for (std::size_t band = 0; band < K_BANDS; ++band)
        {
            const double centre = SushiDSP::room::K_BAND_CENTRES_HZ[band];
            const auto first =
                static_cast<std::size_t>(std::ceil(centre / std::sqrt(2.0) / bin_hz));
            const auto last = static_cast<std::size_t>(std::ceil(centre * std::sqrt(2.0) / bin_hz));
            double sum = 0.0;
            for (std::size_t bin = first; bin < last; ++bin)
            {
                sum += static_cast<double>(real[bin]) * real[bin] +
                       static_cast<double>(imaginary[bin]) * imaginary[bin];
            }
            energy[band] = sum / static_cast<double>(last - first);
        }
        return energy;
    }

    /// @brief Holds what one room's tail renders to and what it was designed for.
    struct Rendered
    {
            /// @brief Stores the remainder the field holds beyond the reflections per band.
            Bands remainder;

            /// @brief Stores the energy of the reflections per band.
            Bands reflected;

            /// @brief Stores the rendered energy per band.
            Bands rendered;
    };

    /// @brief Returns a room's rendering, made once: it takes 131072 frames and a transform.
    const Rendered& rendered_for(const Room& room)
    {
        static std::map<std::string, Rendered> cache;
        const auto found = cache.find(room.name);
        if (found != cache.end())
        {
            return found->second;
        }
        const RoomScene scene = room.scene();
        const EarlyTapSet early = SushiDSP::room::ImageSourceSolver::solve(scene, {});
        const FdnDesign design =
            LateReverbDesign::design(scene, early, K_SAMPLE_RATE, ListenerKind::Ears);
        Rendered made{};
        made.remainder = remainder_of(scene, early, 1.0);
        made.reflected = reflected_energy(early);
        made.rendered = rendered_energy(design);
        return cache.emplace(room.name, made).first->second;
    }

    /// @brief Returns how far past its limit the worst band with a remainder renders from it, in
    /// decibels; at most 0 when every band is inside.
    double worst_render_excess_db(const Rendered& room)
    {
        double worst = -std::numeric_limits<double>::infinity();
        for (std::size_t band = 0; band < K_BANDS; ++band)
        {
            if (room.remainder[band] == 0.0)
            {
                continue;
            }
            const double limit = band == 0 ? K_RENDER_LOW_BAND_DB : K_RENDER_BAND_DB;
            const double error_db = decibels(room.rendered[band]) - decibels(room.remainder[band]);
            worst = std::max(worst, std::fabs(error_db) - limit);
        }
        return worst;
    }

    /// @brief Returns how far under the reflections the loudest band with no remainder renders,
    /// in decibels; infinite when every band has a remainder.
    double smallest_empty_band_margin_db(const Rendered& room)
    {
        double smallest = std::numeric_limits<double>::infinity();
        for (std::size_t band = 0; band < K_BANDS; ++band)
        {
            if (room.remainder[band] != 0.0)
            {
                continue;
            }
            smallest = std::min(smallest,
                                decibels(room.reflected[band]) - decibels(room.rendered[band]));
        }
        return smallest;
    }

    /// @brief Holds one room's scene, its image set and its design for the ears.
    class LateReverbDesignRoom : public ::testing::TestWithParam<Room>
    {
        protected:
            /// @brief Solves the room and designs its tail.
            void SetUp() override
            {
                scene = GetParam().scene();
                early = SushiDSP::room::ImageSourceSolver::solve(scene, {});
                design = LateReverbDesign::design(scene, early, K_SAMPLE_RATE, ListenerKind::Ears);
            }

            /// @brief Stores the room.
            RoomScene scene;

            /// @brief Stores the room's image set.
            EarlyTapSet early{};

            /// @brief Stores the room's design for the ears.
            FdnDesign design{};
    };
} // namespace

class Small_LateReverbDesignRooms : public LateReverbDesignRoom
{
};

class Small_LateReverbDesignTailRooms : public LateReverbDesignRoom
{
};

class Medium_LateReverbDesignRender : public ::testing::TestWithParam<Room>
{
};

class Small_LateReverbDesignCoherence : public ::testing::TestWithParam<Frequency>
{
};

TEST(Small_LateReverbDesign, LinesTotalTheModalOverlapRuleForTheGarage)
{
    // The lines total at least 1.4 * RT60(500 Hz). See tests/nodes/room/README.md.
    EXPECT_GE(total_frames(garage_design().line_frames), K_GARAGE_OVERLAP_FRAMES);
}

TEST(Small_LateReverbDesign, LinesDoNotTotalMoreThanTheCeilingForTheGarage)
{
    EXPECT_LT(total_frames(garage_design().line_frames), K_GARAGE_TOTAL_CEILING_FRAMES);
}

TEST(Small_LateReverbDesign, ShortestLineIsTheFirstPrimeAtTheHandoverTime)
{
    // The shortest line is the handover time. See tests/nodes/room/README.md.
    EXPECT_EQ(garage_design().line_frames.front(), K_GARAGE_SHORTEST_LINE);
}

TEST_P(Small_LateReverbDesignRooms, LinesTotalTheModalOverlapRuleFromTheirOwnMidBandDecay)
{
    const double rt60_mid = SushiDSP::room::EyringDecay::rt60_seconds(scene)[2];
    EXPECT_GE(static_cast<double>(total_frames(design.line_frames)),
              1.4 * rt60_mid * K_SAMPLE_RATE);
}

TEST_P(Small_LateReverbDesignRooms, TailOnsetLandsBetweenTheHandoverTimeAndThePrimeAboveIt)
{
    // The tail lands at the delay plus the shortest line. See tests/nodes/room/README.md.
    const double handover = handover_frames(early);
    const double onset = static_cast<double>(design.delay_to_crossover_frames) +
                         static_cast<double>(design.line_frames.front());
    EXPECT_GE(onset, handover);
    const std::uint32_t prime_above = next_prime_at_or_above(static_cast<std::uint32_t>(handover));
    EXPECT_LE(onset, static_cast<double>(prime_above));
}

INSTANTIATE_TEST_SUITE_P(Rooms, Small_LateReverbDesignRooms, ::testing::ValuesIn(kAllRooms),
                         room_name);

TEST(Small_LateReverbDesign, GarageLongestLineIsTheFirstPrimeAtItsTarget)
{
    // The garage's primes run from 1753 to 10211. See tests/nodes/room/README.md.
    EXPECT_EQ(garage_design().line_frames.back(), K_GARAGE_LONGEST_LINE);
}

TEST(Small_LateReverbDesign, EveryLineIsPrime)
{
    const FdnDesign design = garage_design();
    EXPECT_TRUE(std::all_of(design.line_frames.begin(), design.line_frames.end(), is_prime));
}

TEST(Small_LateReverbDesign, NoTwoLinesShareALength)
{
    const FdnDesign design = garage_design();
    const std::set<std::uint32_t> distinct(design.line_frames.begin(), design.line_frames.end());
    EXPECT_EQ(distinct.size(), K_LINES);
}

TEST(Small_LateReverbDesign, NoLineOutrunsTheStorageTheRendererIsSizedFor)
{
    EXPECT_LE(static_cast<double>(garage_design().line_frames.back()),
              FdnDesign::K_MAX_LINE_SECONDS * K_SAMPLE_RATE);
}

TEST(Small_LateReverbDesign, LineFramesStepsTheTargetsUpToThePrimes)
{
    // The targets 4800 and 19200 step up to 4801 and 19207. See tests/nodes/room/README.md.
    const std::array<std::uint32_t, K_LINES> lines =
        LateReverbDesign::line_frames(0.1, 4.0, K_SAMPLE_RATE);
    EXPECT_EQ(lines.front(), 4801u);
    EXPECT_EQ(lines.back(), 19207u);
}

TEST(Small_LateReverbDesign, LineFramesGivesTheFirstSixteenPrimesForAZeroShortestLine)
{
    // A zero shortest line yields the first sixteen primes. See tests/nodes/room/README.md.
    const std::array<std::uint32_t, K_LINES> first_primes = {2,  3,  5,  7,  11, 13, 17, 19,
                                                             23, 29, 31, 37, 41, 43, 47, 53};
    EXPECT_EQ(LateReverbDesign::line_frames(0.0, 2.0, K_SAMPLE_RATE), first_primes);
}

TEST(Small_LateReverbDesign, LineFiltersFitTheDecayEachBandAsks)
{
    // The cascade lands within 1 dB of each wanted gain. See tests/nodes/room/README.md.
    const Bands rt60 = SushiDSP::room::EyringDecay::rt60_seconds(SushiDSP::test::garage_scene());
    EXPECT_LE(worst_fit_miss_db(garage_design(), rt60), K_FIT_CLEARANCE_DB);
}

TEST(Small_LateReverbDesign, DelayIsZeroWhenTheShortestLineIsPastTheHandoverTime)
{
    // The delay is the handover time less the shortest line. See tests/nodes/room/README.md.
    EXPECT_EQ(garage_design().delay_to_crossover_frames, 0.0f);
}

TEST_P(Small_LateReverbDesignTailRooms, InputGainIsTheRootOfTheMeanShare)
{
    // Each band's share is floored at -30 dB of the largest. See tests/nodes/room/README.md.
    const TailShare tail =
        tail_share(remainder_of(scene, early, 1.0), network_energy(scene, design));
    EXPECT_TRUE(TestSupport::is_within(
        TestSupport::measure_relative_error<float>(design.input_gain, std::sqrt(tail.mean), 2)));
}

TEST_P(Small_LateReverbDesignTailRooms, ToneCorrectionFollowsEachBandsShareOfTheMean)
{
    const TailShare tail =
        tail_share(remainder_of(scene, early, 1.0), network_energy(scene, design));
    EXPECT_LE(worst_tone_miss_db(design, tail), K_TONE_FIT_DB);
}

INSTANTIATE_TEST_SUITE_P(Rooms, Small_LateReverbDesignTailRooms, ::testing::ValuesIn(kTailRooms),
                         room_name);

TEST(Small_LateReverbDesign, BedroomFourKilohertzRemainderSitsOnTheFloor)
{
    const RoomScene bedroom = SushiDSP::test::bedroom_scene();
    const EarlyTapSet early = SushiDSP::room::ImageSourceSolver::solve(bedroom, {});
    EXPECT_EQ(remainder_of(bedroom, early, 1.0)[5], 0.0);
}

TEST(Small_LateReverbDesign, GarageFourKilohertzRemainderIsAboveTheFloor)
{
    EXPECT_GT(
        remainder_of(SushiDSP::test::garage_scene(), garage_early_taps(), 1.0)[5], 0.0);
}

TEST(Small_LateReverbDesign, InputGainGrowsAsTheTapsShrink)
{
    // The tail's gain grows as the taps shrink. See tests/nodes/room/README.md.
    const float full = garage_gain_with_taps_scaled(1.0f);
    const float half = garage_gain_with_taps_scaled(0.5f);
    EXPECT_GT(full, 0.0f);
    EXPECT_GT(half, full);
    EXPECT_GT(garage_gain_with_no_taps(), half);
}

TEST_P(Medium_LateReverbDesignRender, EachBandWithARemainderRendersWithinItsClearance)
{
    // Each band renders within 1 dB, or 2 dB at 125 Hz. See tests/nodes/room/README.md.
    EXPECT_LE(worst_render_excess_db(rendered_for(GetParam())), 0.0);
}

TEST_P(Medium_LateReverbDesignRender, EachBandWithNoRemainderRendersUnderTheReflections)
{
    EXPECT_GT(smallest_empty_band_margin_db(rendered_for(GetParam())), K_EMPTY_BAND_MARGIN_DB);
}

INSTANTIATE_TEST_SUITE_P(Rooms, Medium_LateReverbDesignRender, ::testing::ValuesIn(kAllRooms),
                         room_name);

TEST(Small_LateReverbDesign, LateReverbDesignMakesTheTailCoherentLowAndIncoherentHigh)
{
    // Coherence is above 0.85 at 125 Hz, below 0.20 at 4 kHz. See tests/nodes/room/README.md.
    const FdnDesign design = garage_design();
    EXPECT_GT(cascade_gain(design.coherence_a, 125.0), 0.85);
    EXPECT_LT(cascade_gain(design.coherence_a, 4000.0), 0.20);
    EXPECT_LT(cascade_gain(design.coherence_b, 125.0), 0.20);
    EXPECT_GT(cascade_gain(design.coherence_b, 4000.0), 0.95);
}

TEST_P(Small_LateReverbDesignCoherence, PairIsPowerComplementary)
{
    // |A|^2 + |B|^2 = 1 at every band centre and the corner. See tests/nodes/room/README.md.
    const FdnDesign design = garage_design();
    const double a = cascade_gain(design.coherence_a, GetParam().hertz);
    const double b = cascade_gain(design.coherence_b, GetParam().hertz);
    EXPECT_NEAR(a * a + b * b, 1.0, K_COMPLEMENT_TOLERANCE);
}

INSTANTIATE_TEST_SUITE_P(Frequencies, Small_LateReverbDesignCoherence,
                         ::testing::ValuesIn(kFrequencies), frequency_name);

TEST(Small_LateReverbDesign, ToneCorrectionBoostsTheBassAndCutsTheTrebleForTheGarage)
{
    // The garage's output cascade runs from +4.7 dB at 125 Hz to -12.1 dB at 4 kHz.
    const FdnDesign design = garage_design();
    EXPECT_GT(cascade_db(design.tone_correction, 125.0), 3.5);
    EXPECT_LT(cascade_db(design.tone_correction, 4000.0), -11.0);
}

TEST(Small_LateReverbDesign, ToneCorrectionFallIsTheMeasuredRatioForTheGarage)
{
    const FdnDesign design = garage_design();
    EXPECT_NEAR(cascade_gain(design.tone_correction, 125.0) /
                    cascade_gain(design.tone_correction, 4000.0),
                std::pow(10.0, K_GARAGE_TONE_FALL_DB / 20.0), K_GARAGE_TONE_RATIO_TOLERANCE);
}

TEST(Small_LateReverbDesign, SecondToneSectionCarriesPartOfTheFallToFourKilohertz)
{
    // The second section carries part of the fall to 4 kHz. See tests/nodes/room/README.md.
    const FdnDesign design = garage_design();
    const FdnDesign::Cascade first_only = {design.tone_correction[0], {1.0, 0.0, 0.0, 0.0, 0.0}};
    EXPECT_GT(cascade_db(first_only, 4000.0), cascade_db(design.tone_correction, 4000.0) + 1.0);
}

TEST(Small_LateReverbDesign, MicrophoneCoherenceAIsAPassThrough)
{
    // A microphone's coherence filters are pass-throughs. See tests/nodes/room/README.md.
    const FdnDesign::Section unity = {1.0, 0.0, 0.0, 0.0, 0.0};
    EXPECT_EQ(garage_microphone_design().coherence_a, (FdnDesign::Cascade{unity, unity}));
}

TEST(Small_LateReverbDesign, MicrophoneCoherenceBIsAPassThrough)
{
    const FdnDesign::Section unity = {1.0, 0.0, 0.0, 0.0, 0.0};
    EXPECT_EQ(garage_microphone_design().coherence_b, (FdnDesign::Cascade{unity, unity}));
}

TEST(Small_LateReverbDesign, MicrophoneLinesAreTheRoomsLinesAsForTheEars)
{
    EXPECT_EQ(garage_microphone_design().line_frames, garage_design().line_frames);
}

class Small_LateReverbDesignMicrophone : public ::testing::Test
{
    protected:
        /// @brief Designs the garage's tail for an omni and a cardioid at the listener's spot.
        void SetUp() override
        {
            omni_scene = SushiDSP::test::garage_scene();
            omni_scene.microphone_position = omni_scene.listener_position;
            omni_scene.microphone_pattern = SushiDSP::room::PolarPattern::Omni;
            cardioid_scene = omni_scene;
            cardioid_scene.microphone_pattern = SushiDSP::room::PolarPattern::Cardioid;
            const SushiDSP::room::ImageSourceSolver::Options options{4u, ListenerKind::Microphone,
                                                                     K_SAMPLE_RATE};
            omni_early = SushiDSP::room::ImageSourceSolver::solve(omni_scene, options);
            cardioid_early = SushiDSP::room::ImageSourceSolver::solve(cardioid_scene, options);
            omni_design = LateReverbDesign::design(omni_scene, omni_early, K_SAMPLE_RATE,
                                                   ListenerKind::Microphone);
            cardioid_design = LateReverbDesign::design(cardioid_scene, cardioid_early,
                                                       K_SAMPLE_RATE, ListenerKind::Microphone);
        }

        /// @brief Stores the garage with an omni microphone at the listener.
        RoomScene omni_scene;

        /// @brief Stores the garage with a cardioid microphone at the listener.
        RoomScene cardioid_scene;

        /// @brief Stores the omni's image set.
        EarlyTapSet omni_early{};

        /// @brief Stores the cardioid's image set.
        EarlyTapSet cardioid_early{};

        /// @brief Stores the omni's design.
        FdnDesign omni_design{};

        /// @brief Stores the cardioid's design.
        FdnDesign cardioid_design{};
};

TEST_F(Small_LateReverbDesignMicrophone, OmniAtTheListenersSpotGetsTheEarsInputGain)
{
    // A microphone picks up a^2 + (1 - a)^2 / 3 of the field. See tests/nodes/room/README.md.
    const double ears = static_cast<double>(garage_design().input_gain);
    EXPECT_NEAR(static_cast<double>(omni_design.input_gain), ears,
                K_OMNI_VERSUS_EARS_TOLERANCE * ears);
}

TEST_F(Small_LateReverbDesignMicrophone, CardioidInputGainFollowsAThirdOfTheField)
{
    // The design's gain follows that arithmetic. See tests/nodes/room/README.md.
    const TailShare tail = tail_share(remainder_of(cardioid_scene, cardioid_early, 1.0 / 3.0),
                                      network_energy(cardioid_scene, cardioid_design));
    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_relative_error<float>(
        cardioid_design.input_gain, std::sqrt(tail.mean), 2)));
}

TEST_F(Small_LateReverbDesignMicrophone, CardioidInputGainIsBelowTheOmnis)
{
    EXPECT_LT(cardioid_design.input_gain, omni_design.input_gain);
}
