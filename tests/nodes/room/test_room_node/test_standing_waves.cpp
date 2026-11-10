/**************************************************************************/
/* test_standing_waves.cpp                                                */
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
 * @file test_standing_waves.cpp
 * @brief Tests for RoomNode: the standing waves of the garage it rings at, the widths they have,
 * the bass a corner presses in, and how they cross a publish.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include <SushiDSP/nodes/room/modal_solver.hpp>
#include <SushiDSP/nodes/room/room_node.hpp>

#include "nodes/room/test_room_node/room_node_rig.hpp"

namespace
{
    using namespace SushiDSP::test::room_node_rig;
    using SushiDSP::room::RoomScene;

    /// @brief Holds the impulse render the full standing wave tests transform: 262144 frames,
    /// 5.46 s, with bins of 0.183 Hz against widths of 0.82 Hz and up.
    constexpr std::uint32_t K_LONG_FFT = 262144;

    /// @brief Holds the impulse render the medium standing wave tests transform: 65536 frames,
    /// 1.37 s, with bins of 0.73 Hz. The first mode's width is 1.485 Hz, a decay of 1.48 s, so
    /// the render keeps all but 10^-5.6 of its energy.
    constexpr std::uint32_t K_SHORT_FFT = 65536;

    /// @brief Holds the prominence a spectral peak must stand at above its valleys, in dB.
    constexpr double K_PROMINENCE_DB = 6.0;

    /// @brief Holds the highest frequency the garage's first six modes lie under, in hertz: the
    /// seventh, the (0, 1, 1) mode, is at 80.9 Hz.
    constexpr double K_SIX_MODE_LIMIT_HZ = 78.0;

    /// @brief Holds the highest frequency the first five modes lie under, in hertz: the sixth
    /// stands 2.86 Hz above the fifth with widths of 1.5 and 0.84 Hz, so their contours meet.
    constexpr double K_FIVE_MODE_LIMIT_HZ = 70.0;

    /// @brief Holds the highest frequency the first mode alone lies under, in hertz.
    constexpr double K_ONE_MODE_LIMIT_HZ = 40.0;

    /// @brief Bounds the relative distance of a peak from its mode's frequency.
    constexpr double K_PEAK_RELATIVE_TOLERANCE = 0.005;

    /// @brief Bounds the relative distance of a peak's width from the width its decay gives.
    constexpr double K_WIDTH_RELATIVE_TOLERANCE = 0.2;

    /// @brief Holds the least a corner listener hears over a centre one in the bass, in dB.
    constexpr double K_CORNER_LIFT_DB = 12.0;

    /// @brief Holds the lower edge of the bass band, in hertz.
    constexpr double K_BASS_LOWER_HZ = 20.0;

    /// @brief Holds the upper edge of the bass band, in hertz.
    constexpr double K_BASS_UPPER_HZ = 100.0;

    /// @brief Holds the largest step ratio a fade that keeps its state may read.
    constexpr double K_STEP_RATIO_CEILING = 1.01;

    /// @brief Bounds how far the energy after a fade that kept its bank is from the arriving
    /// scene's steady render, in dB.
    constexpr double K_SETTLED_BAND_DB = 0.5;

    /// @brief Bounds the step ratio of a fade that is an exact blend, beyond one.
    constexpr double K_EXACT_BLEND_STEP_EXCESS = 1e-6;

    /// @brief Bounds the residual of a fade of the ear gains against the blend of two steady
    /// renders, in dB under the blend's peak: the geometric layers' leakage of the tone.
    constexpr double K_EAR_GAIN_RESIDUAL_CEILING_DB = -70.0;

    /// @brief Bounds the energy after a fade that rebuilt its bank from zero, against the
    /// steady render, in dB: a quarter.
    constexpr double K_REBUILT_BANK_CEILING_DB = -6.0;

    /// @brief Holds the mode of the room the fade tests drive: (0, 0, 1), at 68.6 Hz.
    constexpr double K_DRIVEN_MODE_HZ = 68.6;

    /// @brief Holds the block the full fade tests apply the publish before: 200 blocks, 1.07 s,
    /// five time constants of the driven mode's build-up.
    constexpr std::uint32_t K_LONG_SWAP_BLOCK = 200;

    /// @brief Holds the block the medium fade tests apply the publish before. The steady render
    /// has built up by then to 1 - e^(-t / tau) of its level with tau at most 0.39 s, which is
    /// 0.67 at 64 blocks; the bank that restarts at the publish is behind the steady one for
    /// every swap, so the direction of each bound holds at this swap.
    constexpr std::uint32_t K_SHORT_SWAP_BLOCK = 64;

    /// @brief Holds the blocks a fade render runs past its swap: the ears' fade starts one
    /// partition later, lasts K_CROSSFADE_FRAMES and is measured four blocks past it, 5.5 blocks.
    constexpr std::uint32_t K_BLOCKS_PAST_SWAP = 8;

    /// @brief Returns the frequency of one bin of a transform of the given size.
    double bin_hz(double bin, std::uint32_t size)
    {
        return bin * K_SAMPLE_RATE / static_cast<double>(size);
    }

    /// @brief Returns 10 log10 of the mean power per hertz between two frequencies.
    double band_density_db(const std::vector<double>& power_db, std::uint32_t size, double lower_hz,
                           double upper_hz)
    {
        const std::size_t first = static_cast<std::size_t>(std::ceil(lower_hz / bin_hz(1.0, size)));
        const std::size_t last = static_cast<std::size_t>(std::ceil(upper_hz / bin_hz(1.0, size)));
        double sum = 0.0;
        for (std::size_t bin = first; bin < last; ++bin)
        {
            sum += std::pow(10.0, power_db[bin] / 10.0);
        }
        return 10.0 * std::log10(sum / static_cast<double>(last - first));
    }

    /// @brief Holds one spectral peak: its interpolated frequency and its -3 dB width.
    struct SpectralPeak
    {
            /// @brief Stores the parabolic-interpolated frequency, in hertz.
            double frequency_hz;

            /// @brief Stores the width between the two frequencies 3 dB under the peak, in hertz.
            double width_hz;
    };

    /// @brief Returns every local maximum of the spectrum between two frequencies that stands
    /// at least prominence_db above the lowest bin between it and its neighbours on both
    /// sides, in frequency order.
    std::vector<SpectralPeak> find_peaks(const std::vector<double>& power_db, std::uint32_t size,
                                         double lower_hz, double upper_hz, double prominence_db)
    {
        const std::size_t first = static_cast<std::size_t>(std::ceil(lower_hz / bin_hz(1.0, size)));
        const std::size_t last = static_cast<std::size_t>(std::floor(upper_hz / bin_hz(1.0, size)));
        std::vector<SpectralPeak> peaks;
        for (std::size_t bin = first; bin <= last; ++bin)
        {
            const double at = power_db[bin];
            if (!(at > power_db[bin - 1] && at >= power_db[bin + 1]))
            {
                continue;
            }
            // Prominence is measured to the deeper valley. See tests/nodes/room/test_room_node/README.md.
            double left_floor = at;
            for (std::size_t probe = bin; probe > 0 && power_db[probe] <= at; --probe)
            {
                left_floor = std::min(left_floor, power_db[probe]);
            }
            double right_floor = at;
            for (std::size_t probe = bin; probe + 1 < power_db.size() && power_db[probe] <= at;
                 ++probe)
            {
                right_floor = std::min(right_floor, power_db[probe]);
            }
            if (at - std::max(left_floor, right_floor) < prominence_db)
            {
                continue;
            }
            const double below = power_db[bin - 1];
            const double above = power_db[bin + 1];
            const double offset = 0.5 * (below - above) / (below - 2.0 * at + above);
            std::size_t low = bin;
            while (low > 0 && power_db[low] > at - 3.0)
            {
                --low;
            }
            std::size_t high = bin;
            while (high + 1 < power_db.size() && power_db[high] > at - 3.0)
            {
                ++high;
            }
            const double low_edge =
                static_cast<double>(low) +
                (at - 3.0 - power_db[low]) / (power_db[low + 1] - power_db[low]);
            const double high_edge =
                static_cast<double>(high) -
                (at - 3.0 - power_db[high]) / (power_db[high - 1] - power_db[high]);
            peaks.push_back({bin_hz(static_cast<double>(bin) + offset, size),
                             bin_hz(high_edge - low_edge, size)});
        }
        return peaks;
    }

    /// @brief Returns the power spectrum of a scene's impulse response heard by the ears, the
    /// left output over size frames.
    std::vector<double> wave_spectrum_db(const RoomScene& scene, std::uint32_t size)
    {
        std::unique_ptr<SushiDSP::room::RoomNode> node = prepared_node(scene, K_LISTENER_EARS);
        return power_spectrum_db(render(*node, impulse(size)), size);
    }

    /// @brief Returns the peaks of the wave scene's long spectrum under the six-mode limit, made
    /// once.
    const std::vector<SpectralPeak>& long_peaks()
    {
        static const std::vector<SpectralPeak> peaks = find_peaks(
            wave_spectrum_db(wave_scene(), K_LONG_FFT), K_LONG_FFT, K_BASS_LOWER_HZ,
            K_SIX_MODE_LIMIT_HZ, K_PROMINENCE_DB);
        return peaks;
    }

    /// @brief Returns the peaks of the wave scene's long spectrum under the five-mode limit,
    /// made once.
    const std::vector<SpectralPeak>& long_five_peaks()
    {
        static const std::vector<SpectralPeak> peaks = find_peaks(
            wave_spectrum_db(wave_scene(), K_LONG_FFT), K_LONG_FFT, K_BASS_LOWER_HZ,
            K_FIVE_MODE_LIMIT_HZ, K_PROMINENCE_DB);
        return peaks;
    }

    /// @brief Returns the corner listener's bass over the centre listener's, in dB, over a
    /// render of the given size. Every mode with an odd index has a node at the centre.
    double corner_lift_db(std::uint32_t size)
    {
        RoomScene corner = wave_scene();
        corner.listener_position = {0.15, 0.15, 0.15};
        RoomScene centre = wave_scene();
        centre.listener_position = {3.0, 2.0, 1.25};
        return band_density_db(wave_spectrum_db(corner, size), size, K_BASS_LOWER_HZ,
                               K_BASS_UPPER_HZ) -
               band_density_db(wave_spectrum_db(centre, size), size, K_BASS_LOWER_HZ,
                               K_BASS_UPPER_HZ);
    }

    /// @brief Renders the tone of the driven mode through three nodes heard by the ears: one in
    /// scene a throughout, one in scene b throughout, and one that moves from a to b at the swap
    /// block; and measures the fade, which starts one partition after the swap block.
    FadeMeasure wave_fade(const RoomScene& scene_b, std::uint32_t swap_block)
    {
        const RoomScene scene_a = wave_scene();
        const std::vector<float> input =
            sine(K_DRIVEN_MODE_HZ, static_cast<std::size_t>(swap_block + K_BLOCKS_PAST_SWAP) * K_BLOCK);
        const std::vector<float> steady_a = render(*prepared_node(scene_a, K_LISTENER_EARS), input);
        const std::vector<float> steady_b = render(*prepared_node(scene_b, K_LISTENER_EARS), input);
        const std::vector<float> moving = render(*prepared_node(scene_a, K_LISTENER_EARS), input,
                                                 {swap_block}, scene_edit(scene_b));
        return measure_fade(steady_a, steady_b, moving,
                            static_cast<std::size_t>(swap_block) * K_BLOCK + K_ALIGN_FRAMES);
    }

    /// @brief Returns the wave scene with the cabinet moved 0.2 m along x.
    RoomScene cabinet_moved_scene()
    {
        RoomScene scene = wave_scene();
        scene.source_position.x += 0.2;
        return scene;
    }

    /// @brief Returns the wave scene with the listener moved 0.3 m along -y.
    RoomScene listener_moved_scene()
    {
        RoomScene scene = wave_scene();
        scene.listener_position.y -= 0.3;
        return scene;
    }

    /// @brief Returns the wave scene with the room 0.1 m wider.
    RoomScene widened_scene()
    {
        RoomScene scene = wave_scene();
        scene.dimensions.x += 0.1;
        return scene;
    }

    /// @brief Returns the fade of the long swap to a moved cabinet, made once.
    const FadeMeasure& long_cabinet_fade()
    {
        static const FadeMeasure measure = wave_fade(cabinet_moved_scene(), K_LONG_SWAP_BLOCK);
        return measure;
    }

    /// @brief Returns the fade of the long swap to a moved listener, made once.
    const FadeMeasure& long_listener_fade()
    {
        static const FadeMeasure measure = wave_fade(listener_moved_scene(), K_LONG_SWAP_BLOCK);
        return measure;
    }
} // namespace

class Large_RoomNodeModeFrequency : public ::testing::TestWithParam<std::size_t>
{
};

TEST_P(Large_RoomNodeModeFrequency, RoomNodeRingsAtTheGaragesEigenfrequency)
{
    // The first six modes lie between 28.583 and 71.458 Hz.
    const std::vector<SushiDSP::room::ModalSolver::Mode> modes =
        SushiDSP::room::ModalSolver::enumerate(wave_scene(), K_SIX_MODE_LIMIT_HZ);
    ASSERT_EQ(long_peaks().size(), modes.size());
    const double expected = modes[GetParam()].frequency_hz;
    EXPECT_NEAR(long_peaks()[GetParam()].frequency_hz, expected,
                K_PEAK_RELATIVE_TOLERANCE * expected);
}

INSTANTIATE_TEST_SUITE_P(Modes, Large_RoomNodeModeFrequency,
                         ::testing::Values(std::size_t{0}, std::size_t{1}, std::size_t{2},
                                           std::size_t{3}, std::size_t{4}, std::size_t{5}));

class Large_RoomNodeModeWidth : public ::testing::TestWithParam<std::size_t>
{
};

TEST_P(Large_RoomNodeModeWidth, RoomNodeModeWidthMatchesItsDamping)
{
    // A resonator's -3 dB width is 3 ln(10) / (pi T60), 2.199 / T60 Hz.
    const std::vector<SushiDSP::room::ModalSolver::Mode> modes =
        SushiDSP::room::ModalSolver::enumerate(wave_scene(), K_FIVE_MODE_LIMIT_HZ);
    ASSERT_EQ(long_five_peaks().size(), modes.size());
    const double expected = 3.0 * std::log(10.0) / (K_PI * modes[GetParam()].decay_seconds);
    EXPECT_NEAR(long_five_peaks()[GetParam()].width_hz, expected,
                K_WIDTH_RELATIVE_TOLERANCE * expected);
}

INSTANTIATE_TEST_SUITE_P(Modes, Large_RoomNodeModeWidth,
                         ::testing::Values(std::size_t{0}, std::size_t{1}, std::size_t{2},
                                           std::size_t{3}, std::size_t{4}));

TEST(Medium_RoomNodeFirstMode, RoomNodeRingsAtTheGaragesLowestEigenfrequency)
{
    // One bin holds the peak, and the interpolation moves it by half a bin at most.
    const std::vector<SushiDSP::room::ModalSolver::Mode> modes =
        SushiDSP::room::ModalSolver::enumerate(wave_scene(), K_ONE_MODE_LIMIT_HZ);
    ASSERT_EQ(modes.size(), 1u);
    const std::vector<SpectralPeak> peaks =
        find_peaks(wave_spectrum_db(wave_scene(), K_SHORT_FFT), K_SHORT_FFT, K_BASS_LOWER_HZ,
                   K_ONE_MODE_LIMIT_HZ, K_PROMINENCE_DB);
    ASSERT_EQ(peaks.size(), 1u);
    EXPECT_NEAR(peaks[0].frequency_hz, modes[0].frequency_hz, bin_hz(0.5, K_SHORT_FFT));
}

TEST(Large_RoomNodeCornerBass, RoomNodePressesTheBassIntoTheCorner)
{
    EXPECT_GE(corner_lift_db(K_LONG_FFT), K_CORNER_LIFT_DB);
}

TEST(Medium_RoomNodeCornerBass, RoomNodePressesTheBassIntoTheCornerInAShortRender)
{
    // The slowest mode decays over 2.7 s, so the short render keeps all but 10^-3.1 of its energy.
    EXPECT_GE(corner_lift_db(K_SHORT_FFT), K_CORNER_LIFT_DB);
}

TEST(Large_RoomNodeWaveFade, RoomNodeCarriesTheStandingWavesAcrossAMovedCabinetWithoutAStep)
{
    // The bank keeps its state; only the source gains ramp.
    EXPECT_LE(long_cabinet_fade().step_ratio, K_STEP_RATIO_CEILING);
}

TEST(Large_RoomNodeWaveFade, RoomNodeSettlesAMovedCabinetToTheSteadyRender)
{
    EXPECT_NEAR(long_cabinet_fade().settled_db, 0.0, K_SETTLED_BAND_DB);
}

TEST(Large_RoomNodeWaveFade, RoomNodeRampsTheEarGainsOfAMovedListenerWithoutAStep)
{
    // The bank's output is linear in the ear gains.
    EXPECT_LE(long_listener_fade().step_ratio, 1.0 + K_EXACT_BLEND_STEP_EXCESS);
}

TEST(Large_RoomNodeWaveFade, RoomNodeRampsTheEarGainsAsTheOutputBlendWould)
{
    const FadeMeasure& measure = long_listener_fade();
    EXPECT_LT(to_decibels(measure.residual / measure.peak), K_EAR_GAIN_RESIDUAL_CEILING_DB);
}

TEST(Large_RoomNodeWaveFade, RoomNodeRebuildsTheStandingWavesInAChangedRoom)
{
    // A changed room starts the spare bank from zero.
    EXPECT_LT(wave_fade(widened_scene(), K_LONG_SWAP_BLOCK).settled_db, K_REBUILT_BANK_CEILING_DB);
}

TEST(Medium_RoomNodeWaveFade, RoomNodeCarriesTheStandingWavesAcrossAMovedCabinetWithoutAStepEarly)
{
    EXPECT_LE(wave_fade(cabinet_moved_scene(), K_SHORT_SWAP_BLOCK).step_ratio,
              K_STEP_RATIO_CEILING);
}

TEST(Medium_RoomNodeWaveFade, RoomNodeRebuildsTheStandingWavesInAChangedRoomEarly)
{
    EXPECT_LT(wave_fade(widened_scene(), K_SHORT_SWAP_BLOCK).settled_db,
              K_REBUILT_BANK_CEILING_DB);
}
