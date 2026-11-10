/**************************************************************************/
/* test_ears.cpp                                                          */
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
 * @file test_ears.cpp
 * @brief Tests for RoomNode's ears port: the direct sound, the cabinet's beam, the direct-delay
 * choice, the interaural delay, the HRTF source and the tail's coherence.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <SushiDSP/foundation/math/fft.hpp>
#include <SushiDSP/nodes/room/cabinet_directivity.hpp>
#include <SushiDSP/nodes/room/hrtf_set.hpp>
#include <SushiDSP/nodes/room/hrtf_set_loader.hpp>
#include <SushiDSP/nodes/room/room_node.hpp>
#include <SushiDSP/nodes/room/room_render_set.hpp>
#include <SushiDSP/nodes/room/spherical_head_hrtf.hpp>
#include <SushiDSP/nodes/room/stage_limits.hpp>

#include "nodes/room/test_room_node/room_node_rig.hpp"

namespace
{
    using namespace SushiDSP::test::room_node_rig;
    using SushiDSP::room::RoomScene;

    /// @brief Holds the frames of the direct-sound renders: 4096, which hold the floor image.
    constexpr std::size_t K_DIRECT_RENDER_FRAMES = 4096;

    /// @brief Holds the frames the ears' silence is read over: the alignment delay less the three
    /// frames the first kernel tap reaches back.
    constexpr std::size_t K_SILENT_FRAMES = K_ALIGN_FRAMES - 3;

    /// @brief Bounds how far the direct peak is from the compensated delay, in frames.
    constexpr double K_PEAK_FRAME_TOLERANCE = 2.0;

    /// @brief Holds the frames either side of the compensated delay the peak is searched in.
    constexpr long K_PEAK_WINDOW_FRAMES = 3;

    /// @brief Holds the least the reference's peak may be, so a silent pair cannot agree.
    constexpr double K_AUDIBLE_PEAK = 0.1;

    /// @brief Bounds the node's direct sound against the reference, as a fraction of the peak.
    constexpr double K_REFERENCE_RELATIVE_TOLERANCE = 1e-5;

    /// @brief Bounds the difference of the two ears in the median plane, as a fraction of the peak.
    constexpr double K_EAR_SYMMETRY_RELATIVE_TOLERANCE = 1e-4;

    /// @brief Holds the fraction of a window's peak that marks an onset in the direct window.
    constexpr double K_DIRECT_ONSET_FRACTION = 1e-4;

    /// @brief Holds the fraction of a window's peak that marks the floor image's onset.
    constexpr double K_FLOOR_ONSET_FRACTION = 1e-3;

    /// @brief Bounds how far an onset is from the delay the geometry gives, in frames. The onset
    /// precedes the peak by the half width of the kernel and the fitted filter.
    constexpr double K_ONSET_TOLERANCE_FRAMES = 10.0;

    /// @brief Bounds how far the compensated direct onset is from the compensated delay, in frames.
    constexpr double K_DIRECT_ONSET_TOLERANCE_FRAMES = 3.0;

    /// @brief Holds the distance from the cabinet to the ears in the dead room, in metres.
    constexpr double K_EARS_DISTANCE_M = 2.0;

    /// @brief Holds the dead room's height of the cabinet above its floor, in metres.
    constexpr double K_DEAD_ROOM_HEIGHT_M = 6.0;

    /// @brief Returns the frames the physical direct delay is: the rounded propagation delay of a
    /// path of some metres.
    std::uint32_t rounded_delay(double metres)
    {
        return static_cast<std::uint32_t>(std::lround(metres * K_FRAMES_PER_METRE));
    }

    /// @brief Returns the frames the floor image lands at in the dead room with the propagation
    /// compensated: the alignment delay plus the image's own path, less the direct sound's rounded
    /// delay.
    double floor_image_frames()
    {
        const double path_m = std::sqrt(K_EARS_DISTANCE_M * K_EARS_DISTANCE_M +
                                        4.0 * K_DEAD_ROOM_HEIGHT_M * K_DEAD_ROOM_HEIGHT_M);
        return static_cast<double>(K_ALIGN_FRAMES) + path_m * K_FRAMES_PER_METRE -
               static_cast<double>(rounded_delay(K_EARS_DISTANCE_M));
    }

    /// @brief Returns the index of the local maximum of |ir| nearest a frame, or -1 when no
    /// local maximum above the floor lies within the window.
    long nearest_peak(const std::vector<float>& ir, double frame, long window, float floor)
    {
        long best = -1;
        double best_distance = 1e9;
        const long low = std::max<long>(1, static_cast<long>(std::floor(frame)) - window);
        const long high = std::min<long>(static_cast<long>(ir.size()) - 2,
                                         static_cast<long>(std::ceil(frame)) + window);
        for (long index = low; index <= high; ++index)
        {
            const float here = std::fabs(ir[static_cast<std::size_t>(index)]);
            const bool peak = here > std::fabs(ir[static_cast<std::size_t>(index - 1)]) &&
                              here >= std::fabs(ir[static_cast<std::size_t>(index + 1)]) &&
                              here > floor;
            const double distance = std::fabs(static_cast<double>(index) - frame);
            if (peak && distance < best_distance)
            {
                best_distance = distance;
                best = index;
            }
        }
        return best;
    }

    /// @brief Returns the third-order Lagrange read of an impulse at one fractional delay: the
    /// early renderer's kernel, the sample at the integer delay weighted (1 - frac) and so on.
    std::vector<float> lagrange_impulse(double delay_frames, double gain, std::size_t frames)
    {
        std::vector<float> out(frames, 0.0f);
        const double whole = std::floor(delay_frames);
        const double frac = delay_frames - whole;
        const double s = 2.0 - frac;
        const std::array<double, 4> kernel = {
            -(s - 1.0) * (s - 2.0) * (s - 3.0) / 6.0, s * (s - 2.0) * (s - 3.0) / 2.0,
            -s * (s - 1.0) * (s - 3.0) / 2.0, s * (s - 1.0) * (s - 2.0) / 6.0};
        // Weight k lands an impulse at whole + 2 - k. See tests/nodes/room/test_room_node/README.md.
        for (std::size_t k = 0; k < kernel.size(); ++k)
        {
            const long frame = static_cast<long>(whole) + 2 - static_cast<long>(k);
            if (frame >= 0 && frame < static_cast<long>(frames))
            {
                out[static_cast<std::size_t>(frame)] = static_cast<float>(kernel[k] * gain);
            }
        }
        return out;
    }

    /// @brief Runs one of a tap's sections over a signal in place, double state.
    void run_section(const std::array<double, 5>& section, std::vector<float>& signal)
    {
        double x1 = 0.0, x2 = 0.0, y1 = 0.0, y2 = 0.0;
        for (float& sample : signal)
        {
            const double x = sample;
            const double y = section[0] * x + section[1] * x1 + section[2] * x2 - section[3] * y1 -
                             section[4] * y2;
            x2 = x1;
            x1 = x;
            y2 = y1;
            y1 = y;
            sample = static_cast<float>(y);
        }
    }

    /// @brief Returns what the node's direct path must render for an impulse into one ear: tap 0
    /// of the ears set, compensated as the node publishes it, read past the alignment delay and
    /// the far ear's ITD through the renderer's kernel, its two sections, its gain, then the
    /// ear's minimum-phase HRIR.
    std::vector<float> reference_direct(const SushiDSP::room::RoomRenderSet& set, std::uint32_t ear,
                                        std::size_t frames)
    {
        const SushiDSP::room::EarlyTap& tap = set.early.taps[0];
        const double itd = set.direct_itd_frames;
        const double extra = ear == 0 ? std::max(-itd, 0.0) : std::max(itd, 0.0);
        std::vector<float> signal = lagrange_impulse(
            static_cast<double>(K_ALIGN_FRAMES) + tap.delay_frames + extra, tap.gain, frames);
        run_section(tap.section_a, signal);
        run_section(tap.section_b, signal);
        const std::array<float, SushiDSP::room::HrtfSet::K_TAP_COUNT>& hrir =
            ear == 0 ? set.direct_left : set.direct_right;
        std::vector<float> out(frames, 0.0f);
        for (std::size_t frame = 0; frame < frames; ++frame)
        {
            double sum = 0.0;
            for (std::size_t k = 0; k < hrir.size() && k <= frame; ++k)
            {
                sum += static_cast<double>(hrir[k]) * signal[frame - k];
            }
            out[frame] = static_cast<float>(sum);
        }
        return out;
    }

    /// @brief Holds the dead room's ears render and the reference it is held to.
    struct DirectSound
    {
            /// @brief Stores the node's two outputs over K_DIRECT_RENDER_FRAMES.
            std::array<std::vector<float>, 2> ears;

            /// @brief Stores the reference of the left ear over the direct window.
            std::vector<float> reference_left;

            /// @brief Stores the reference of the right ear over the direct window.
            std::vector<float> reference_right;
    };

    /// @brief Returns the dead room's direct sound, rendered once.
    const DirectSound& direct_sound()
    {
        static const DirectSound sound = []
        {
            const RoomScene scene = centred_dead_room();
            DirectSound made;
            made.ears = render_both(*prepared_node(scene, K_LISTENER_EARS),
                                    impulse(K_DIRECT_RENDER_FRAMES));
            const SushiDSP::room::HrtfSet hrtf =
                SushiDSP::room::SphericalHeadHrtf::build(K_SAMPLE_RATE);
            std::unique_ptr<SushiDSP::room::RoomRenderSet> set = SushiDSP::room::build_render_set(
                scene, hrtf, K_SAMPLE_RATE, SushiDSP::room::ListenerKind::Ears);
            SushiDSP::room::RoomNode::compensate_direct_delay(*set);
            made.reference_left = reference_direct(*set, 0, K_DIRECT_WINDOW);
            made.reference_right = reference_direct(*set, 1, K_DIRECT_WINDOW);
            return made;
        }();
        return sound;
    }

    /// @brief Returns the peak of the left reference over the direct window.
    double reference_peak()
    {
        return peak_of(direct_sound().reference_left, 0, K_DIRECT_WINDOW);
    }

    /// @brief Returns the dead room with the listener on the cabinet's axis, facing it, or at 60
    /// degrees off the axis at the same distance, facing it.
    RoomScene beam_scene(bool off_axis)
    {
        RoomScene scene = centred_dead_room();
        scene.listener_yaw_rad = K_PI;
        if (off_axis)
        {
            scene.listener_position = {7.0, 6.0 + K_EARS_DISTANCE_M * std::sin(K_PI / 3.0), 6.0};
            scene.listener_yaw_rad = std::atan2(6.0 - scene.listener_position.y,
                                                6.0 - scene.listener_position.x);
        }
        return scene;
    }

    /// @brief Returns the third-octave drop at 2 kHz and the move at 125 Hz from the on-axis
    /// listener to the off-axis one, in dB, and the piston model's own two figures.
    struct BeamReading
    {
            /// @brief Stores the rendered fall of the 2 kHz band.
            double drop_2k;

            /// @brief Stores the rendered rise of the 125 Hz band.
            double move_125;

            /// @brief Stores the model's fall of the 2 kHz band.
            double model_2k;

            /// @brief Stores the model's rise of the 125 Hz band.
            double model_125;
    };

    /// @brief Returns the beam reading, rendered once.
    const BeamReading& beam_reading()
    {
        static const BeamReading reading = []
        {
            const std::vector<double> on_db = direct_power_db(beam_scene(false), K_LISTENER_EARS);
            const std::vector<double> off_db = direct_power_db(beam_scene(true), K_LISTENER_EARS);
            const SushiDSP::room::CabinetDirectivity cabinet(SushiDSP::room::K_CABINET4X12);
            const SushiDSP::room::BandResponse model = cabinet.gain_for(K_PI / 3.0, 0.0);
            return BeamReading{third_octave_db(on_db, 2000.0) - third_octave_db(off_db, 2000.0),
                               third_octave_db(off_db, 125.0) - third_octave_db(on_db, 125.0),
                               -20.0 * std::log10(static_cast<double>(model.gain[4])),
                               20.0 * std::log10(static_cast<double>(model.gain[0]))};
        }();
        return reading;
    }

    /// @brief Holds the least the 2 kHz band falls by, in dB: the four pistons beam there.
    constexpr double K_BEAMING_DROP_DB = 6.0;

    /// @brief Bounds the 125 Hz band's move, in dB: the cabinet is omnidirectional there.
    constexpr double K_OMNI_MOVE_DB = 1.0;

    /// @brief Bounds how far a band is from the piston model's, in dB.
    constexpr double K_MODEL_AGREEMENT_DB = 1.0;

    /// @brief Holds the rendered onsets of the dead room under both direct-delay choices.
    struct OnsetRenders
    {
            /// @brief Stores the render with the propagation compensated.
            std::vector<float> compensated;

            /// @brief Stores the render with the propagation physical.
            std::vector<float> physical;
    };

    /// @brief Returns the two renders, made once.
    const OnsetRenders& onset_renders()
    {
        static const OnsetRenders renders = []
        {
            std::unique_ptr<SushiDSP::room::RoomNode> node =
                prepared_node(centred_dead_room(), K_LISTENER_EARS);
            OnsetRenders made;
            made.compensated = render(*node, impulse(K_DIRECT_RENDER_FRAMES));
            node->set_parameter_for_control_thread("direct_delay", K_DIRECT_PHYSICAL);
            node->reset();
            made.physical = render(*node, impulse(K_DIRECT_RENDER_FRAMES));
            return made;
        }();
        return renders;
    }

    /// @brief Returns the distance between the cabinet and the listener of a scene, in metres.
    double source_listener_distance(const RoomScene& scene)
    {
        const double dx = scene.listener_position.x - scene.source_position.x;
        const double dy = scene.listener_position.y - scene.source_position.y;
        const double dz = scene.listener_position.z - scene.source_position.z;
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    }

    /// @brief Returns the path of the six-direction fixture convert_hrtf.py's self-test writes.
    std::string fixture_set_path()
    {
        return std::string(SUSHIDSP_FIXTURE_DIR) + "/hrtf_self_test.hrtf";
    }

    /// @brief Loads the six-direction fixture.
    bool load_fixture(SushiDSP::room::HrtfSet& set, std::string& error)
    {
        return SushiDSP::room::HrtfSetLoader::load(fixture_set_path(), K_SAMPLE_RATE, set, error);
    }

    /// @brief Holds the frames the bare set's decoder is sized to: two ears of four first-order
    /// channels, a decoder the node cannot use.
    constexpr std::size_t K_FIRST_ORDER_DECODER_SIZE =
        2 * 4 * SushiDSP::room::HrtfSet::K_DECODER_TAPS;

    /// @brief Holds the fixture's direction count.
    constexpr std::uint32_t K_FIXTURE_DIRECTIONS = 6;

    /// @brief Holds the frames of the impulse the installed set renders.
    constexpr std::size_t K_HRTF_RENDER_FRAMES = 2048;

    /// @brief Returns a copy of one window of a signal band-limited to one octave, by zeroing
    /// every bin outside it in a transform twice the window's length.
    std::vector<float> octave_band(const std::vector<float>& signal, std::size_t begin,
                                   std::size_t end, double centre_hz)
    {
        std::uint32_t size = 1;
        while (size < 2 * (end - begin))
        {
            size *= 2;
        }
        SushiDSP::Fft fft(size);
        std::vector<float> window(size, 0.0f);
        std::copy(signal.begin() + static_cast<std::ptrdiff_t>(begin),
                  signal.begin() + static_cast<std::ptrdiff_t>(end), window.begin());
        std::vector<float> real(size);
        std::vector<float> imaginary(size);
        fft.forward(window.data(), real.data(), imaginary.data());
        const double hz_per_bin = K_SAMPLE_RATE / static_cast<double>(size);
        const double lower = centre_hz / std::sqrt(2.0);
        const double upper = centre_hz * std::sqrt(2.0);
        for (std::uint32_t bin = 0; bin < size; ++bin)
        {
            const double hz = hz_per_bin * static_cast<double>(bin <= size / 2 ? bin : size - bin);
            if (hz < lower || hz > upper)
            {
                real[bin] = 0.0f;
                imaginary[bin] = 0.0f;
            }
        }
        std::vector<float> band(size);
        fft.inverse(real.data(), imaginary.data(), band.data());
        band.resize(end - begin);
        return band;
    }

    /// @brief Returns the interaural cross-correlation coefficient of two channels over one
    /// window in one octave: the largest normalised cross-correlation within one millisecond.
    double band_iacc(const std::vector<float>& left, const std::vector<float>& right,
                     std::size_t begin, std::size_t end, double centre_hz)
    {
        const std::vector<float> band_left = octave_band(left, begin, end, centre_hz);
        const std::vector<float> band_right = octave_band(right, begin, end, centre_hz);
        const long max_lag = static_cast<long>(std::lround(0.001 * K_SAMPLE_RATE));
        const double norm = std::sqrt(energy(band_left, 0, band_left.size()) *
                                      energy(band_right, 0, band_right.size()));
        double best = 0.0;
        for (long lag = -max_lag; lag <= max_lag; ++lag)
        {
            double sum = 0.0;
            for (std::size_t frame = 0; frame < band_left.size(); ++frame)
            {
                const long partner = static_cast<long>(frame) + lag;
                if (partner >= 0 && partner < static_cast<long>(band_right.size()))
                {
                    sum += static_cast<double>(band_left[frame]) *
                           band_right[static_cast<std::size_t>(partner)];
                }
            }
            best = std::max(best, std::fabs(sum));
        }
        return best / norm;
    }

    /// @brief Holds the frames of the tail window.
    constexpr std::size_t K_TAIL_WINDOW_FRAMES = 16384;

    /// @brief Holds the frames after the taps end that the tail window opens at. The taps end at
    /// the early ring's length, 8397 frames, less the direct sound's rounded delay, plus the
    /// alignment delay; 1024 frames later only the tail is heard. The room's 0.21 s RT60 has
    /// decayed 97 dB by the window's end.
    constexpr std::size_t K_TAIL_MARGIN_FRAMES = 1024;

    /// @brief Returns the first frame of the tail window.
    std::size_t tail_begin()
    {
        return SushiDSP::Room::max_early_delay_frames(K_SAMPLE_RATE) -
               rounded_delay(K_EARS_DISTANCE_M) + K_ALIGN_FRAMES + K_TAIL_MARGIN_FRAMES;
    }

    /// @brief Returns the ears' render of the dead room over the tail window, rendered once.
    const std::array<std::vector<float>, 2>& tail_render()
    {
        static const std::array<std::vector<float>, 2> ears = []
        {
            const std::size_t blocks = (tail_begin() + K_TAIL_WINDOW_FRAMES + K_BLOCK - 1) / K_BLOCK;
            return render_both(*prepared_node(centred_dead_room(), K_LISTENER_EARS),
                               impulse(blocks * K_BLOCK));
        }();
        return ears;
    }

    /// @brief Returns the tail's coherence in one octave.
    double tail_iacc(double centre_hz)
    {
        return band_iacc(tail_render()[0], tail_render()[1], tail_begin(),
                         tail_begin() + K_TAIL_WINDOW_FRAMES, centre_hz);
    }

    /// @brief Holds the least the tail's coherence is in the 125 Hz octave.
    constexpr double K_LOW_IACC_FLOOR = 0.8;

    /// @brief Holds the most the tail's coherence is in the 4 kHz octave.
    constexpr double K_HIGH_IACC_CEILING = 0.3;

    /// @brief Returns the lag of the peak of the cross-correlation of right against left over
    /// one window, parabolic-interpolated, in frames; positive means the right ear is later.
    double interaural_lag(const std::vector<float>& left, const std::vector<float>& right,
                          std::size_t begin, std::size_t end, long max_lag)
    {
        std::vector<double> correlation(static_cast<std::size_t>(2 * max_lag + 1), 0.0);
        for (long lag = -max_lag; lag <= max_lag; ++lag)
        {
            double sum = 0.0;
            for (std::size_t frame = begin; frame < end; ++frame)
            {
                const long partner = static_cast<long>(frame) + lag;
                if (partner >= 0 && partner < static_cast<long>(right.size()))
                {
                    sum +=
                        static_cast<double>(left[frame]) * right[static_cast<std::size_t>(partner)];
                }
            }
            correlation[static_cast<std::size_t>(lag + max_lag)] = sum;
        }
        const std::size_t best = static_cast<std::size_t>(
            std::max_element(correlation.begin(), correlation.end()) - correlation.begin());
        if (best == 0 || best + 1 == correlation.size())
        {
            return static_cast<double>(static_cast<long>(best) - max_lag);
        }
        const double below = correlation[best - 1];
        const double at = correlation[best];
        const double above = correlation[best + 1];
        const double offset = 0.5 * (below - above) / (below - 2.0 * at + above);
        return static_cast<double>(static_cast<long>(best) - max_lag) + offset;
    }

    /// @brief Holds the window the interaural lag is read over: 900 frames, before the nearest
    /// image, off the +x wall at 90 degrees, lands at 1154 - 280 + 128 = 1002.
    constexpr std::size_t K_LAG_WINDOW_FRAMES = 900;

    /// @brief Holds the blocks that cover K_LAG_WINDOW_FRAMES: four of 256 frames are 1024.
    constexpr std::size_t K_LAG_BLOCKS = 4;

    /// @brief Holds the largest lag the correlation is searched over, in frames.
    constexpr long K_MAX_LAG_FRAMES = 60;
} // namespace

TEST(Medium_RoomNodeEarsDirectSound, RoomNodeEarsHearNothingBeforeTheAlignmentDelay)
{
    EXPECT_EQ(peak_of(direct_sound().ears[0], 0, K_SILENT_FRAMES), 0.0);
    EXPECT_EQ(peak_of(direct_sound().ears[1], 0, K_SILENT_FRAMES), 0.0);
}

TEST(Medium_RoomNodeEarsDirectSound, RoomNodeEarsHearThePeakAtTheCompensatedDelay)
{
    const double direct = compensated_direct_frames(K_EARS_DISTANCE_M);
    const long peak = nearest_peak(direct_sound().ears[0], direct, K_PEAK_WINDOW_FRAMES, 1e-3f);
    ASSERT_GE(peak, 0);
    EXPECT_NEAR(static_cast<double>(peak), direct, K_PEAK_FRAME_TOLERANCE);
}

TEST(Medium_RoomNodeEarsDirectSound, RoomNodeLeftEarHearsTheDirectSoundThroughTheHrirPair)
{
    ASSERT_GT(reference_peak(), K_AUDIBLE_PEAK);
    EXPECT_LT(peak_difference(direct_sound().ears[0], direct_sound().reference_left, 0,
                              K_DIRECT_WINDOW),
              K_REFERENCE_RELATIVE_TOLERANCE * reference_peak());
}

TEST(Medium_RoomNodeEarsDirectSound, RoomNodeRightEarHearsTheDirectSoundThroughTheHrirPair)
{
    ASSERT_GT(reference_peak(), K_AUDIBLE_PEAK);
    EXPECT_LT(peak_difference(direct_sound().ears[1], direct_sound().reference_right, 0,
                              K_DIRECT_WINDOW),
              K_REFERENCE_RELATIVE_TOLERANCE * reference_peak());
}

TEST(Medium_RoomNodeEarsDirectSound, RoomNodeEarsAgreeForASourceInTheMedianPlane)
{
    EXPECT_LT(peak_difference(direct_sound().ears[0], direct_sound().ears[1], 0, K_DIRECT_WINDOW),
              K_EAR_SYMMETRY_RELATIVE_TOLERANCE * reference_peak());
}

TEST(Medium_RoomNodeEarsDirectSound, RoomNodeEarsHearTheFloorImageAfterTheDirectWindow)
{
    EXPECT_NEAR(static_cast<double>(onset(direct_sound().ears[0], K_DIRECT_WINDOW,
                                          K_DIRECT_RENDER_FRAMES, K_FLOOR_ONSET_FRACTION)),
                floor_image_frames(), K_ONSET_TOLERANCE_FRAMES);
}

TEST(Medium_RoomNodeCabinetBeam, RoomNodeBeamsTheHighBandAwayFromTheCabinetsAxis)
{
    EXPECT_GT(beam_reading().drop_2k, K_BEAMING_DROP_DB);
}

TEST(Medium_RoomNodeCabinetBeam, RoomNodeHighBandDropMatchesThePistonModel)
{
    EXPECT_NEAR(beam_reading().drop_2k, beam_reading().model_2k, K_MODEL_AGREEMENT_DB);
}

TEST(Medium_RoomNodeCabinetBeam, RoomNodeKeepsTheLowBandLevelAwayFromTheCabinetsAxis)
{
    EXPECT_LT(std::fabs(beam_reading().move_125), K_OMNI_MOVE_DB);
}

TEST(Medium_RoomNodeCabinetBeam, RoomNodeLowBandMoveMatchesThePistonModel)
{
    EXPECT_NEAR(beam_reading().move_125, beam_reading().model_125, K_MODEL_AGREEMENT_DB);
}

TEST(Medium_RoomNodeDirectDelay, RoomNodeDefaultsDirectDelayToCompensated)
{
    SushiDSP::room::RoomNode node;
    node.prepare(K_INFO);
    EXPECT_EQ(node.get_parameter("direct_delay"), K_DIRECT_COMPENSATED);
}

TEST(Medium_RoomNodeDirectDelay, RoomNodeReportsOnePartitionOfLatencyWhenCompensated)
{
    SushiDSP::room::RoomNode node;
    node.prepare(K_INFO);
    EXPECT_EQ(node.latency_samples(), K_ALIGN_FRAMES);
}

TEST(Medium_RoomNodeDirectDelay, RoomNodeAddsTheRoundedPropagationDelayWhenPhysical)
{
    SushiDSP::room::RoomNode node;
    node.prepare(K_INFO);
    const std::uint32_t compensated = node.latency_samples();
    node.set_parameter_for_control_thread("direct_delay", K_DIRECT_PHYSICAL);
    EXPECT_EQ(node.latency_samples() - compensated,
              rounded_delay(source_listener_distance(node.scene())));
}

TEST(Medium_RoomNodeDirectDelay, RoomNodeFollowsThePublishedSceneWhenPhysical)
{
    SushiDSP::room::RoomNode node;
    node.prepare(K_INFO);
    node.set_parameter_for_control_thread("direct_delay", K_DIRECT_PHYSICAL);
    std::string reason;
    ASSERT_TRUE(publish_scene(node, centred_dead_room(), reason)) << reason;
    EXPECT_EQ(node.latency_samples(), K_ALIGN_FRAMES + rounded_delay(K_EARS_DISTANCE_M));
}

TEST(Medium_RoomNodeDirectDelay, RoomNodeDropsThePropagationDelayWhenCompensatedAgain)
{
    SushiDSP::room::RoomNode node;
    node.prepare(K_INFO);
    node.set_parameter_for_control_thread("direct_delay", K_DIRECT_PHYSICAL);
    std::string reason;
    ASSERT_TRUE(publish_scene(node, centred_dead_room(), reason)) << reason;
    node.set_parameter_for_control_thread("direct_delay", K_DIRECT_COMPENSATED);
    EXPECT_EQ(node.latency_samples(), K_ALIGN_FRAMES);
}

TEST(Small_RoomNodeDirectDelay, RoomNodeReportsNoLatencyBeforePrepare)
{
    SushiDSP::room::RoomNode fresh;
    EXPECT_EQ(fresh.latency_samples(), 0u);
}

TEST(Medium_RoomNodeDirectOnset, RoomNodePhysicalTimingMovesTheDirectOnsetByTheRoundedDelay)
{
    const long compensated = onset(onset_renders().compensated, 0, K_DIRECT_WINDOW,
                                   K_DIRECT_ONSET_FRACTION);
    const long physical = onset(onset_renders().physical, 0, K_DIRECT_WINDOW, K_DIRECT_ONSET_FRACTION);
    ASSERT_GE(compensated, 0);
    ASSERT_GE(physical, 0);
    EXPECT_EQ(physical - compensated, static_cast<long>(rounded_delay(K_EARS_DISTANCE_M)));
}

TEST(Medium_RoomNodeDirectOnset, RoomNodeCompensatedOnsetLandsNearTheCompensatedDelay)
{
    const long compensated = onset(onset_renders().compensated, 0, K_DIRECT_WINDOW,
                                   K_DIRECT_ONSET_FRACTION);
    ASSERT_GE(compensated, 0);
    EXPECT_NEAR(static_cast<double>(compensated), compensated_direct_frames(K_EARS_DISTANCE_M),
                K_DIRECT_ONSET_TOLERANCE_FRAMES);
}

TEST(Medium_RoomNodeDirectOnset, RoomNodePhysicalTimingShiftsTheWholeRenderNotTheDirectSoundAlone)
{
    // The floor image's onset moves by the same shift, searched from where it lands.
    const std::size_t shift = rounded_delay(K_EARS_DISTANCE_M);
    const long compensated = onset(onset_renders().compensated, K_DIRECT_WINDOW,
                                   K_DIRECT_RENDER_FRAMES, K_FLOOR_ONSET_FRACTION);
    const long physical = onset(onset_renders().physical, K_DIRECT_WINDOW + shift,
                                K_DIRECT_RENDER_FRAMES, K_FLOOR_ONSET_FRACTION);
    ASSERT_GE(compensated, 0);
    ASSERT_GE(physical, 0);
    EXPECT_EQ(physical - compensated, static_cast<long>(shift));
}

TEST(Medium_RoomNodeDirectOnset, RoomNodeStoresTheAudioThreadsChoiceForTheNextPublish)
{
    const RoomScene scene = centred_dead_room();
    std::unique_ptr<SushiDSP::room::RoomNode> node = prepared_node(scene, K_LISTENER_EARS);
    node->set_parameter_for_control_thread("direct_delay", K_DIRECT_PHYSICAL);
    node->reset();
    const std::vector<float> input = impulse(K_DIRECT_WINDOW_BLOCKS * K_BLOCK);
    const long physical = onset(render(*node, input), 0, K_DIRECT_WINDOW, K_DIRECT_ONSET_FRACTION);

    node->set_parameter_for_audio_thread(std::uint16_t{0}, K_DIRECT_COMPENSATED);
    EXPECT_EQ(node->get_parameter("direct_delay"), K_DIRECT_COMPENSATED);
    node->reset();
    EXPECT_EQ(onset(render(*node, input), 0, K_DIRECT_WINDOW, K_DIRECT_ONSET_FRACTION), physical);
}

TEST(Medium_RoomNodeDirectOnset, RoomNodeAppliesTheStoredChoiceAtTheNextPublish)
{
    const RoomScene scene = centred_dead_room();
    std::unique_ptr<SushiDSP::room::RoomNode> node = prepared_node(scene, K_LISTENER_EARS);
    const std::vector<float> input = impulse(K_DIRECT_WINDOW_BLOCKS * K_BLOCK);
    const long compensated = onset(render(*node, input), 0, K_DIRECT_WINDOW, K_DIRECT_ONSET_FRACTION);
    node->set_parameter_for_control_thread("direct_delay", K_DIRECT_PHYSICAL);
    node->set_parameter_for_audio_thread(std::uint16_t{0}, K_DIRECT_COMPENSATED);
    std::string reason;
    ASSERT_TRUE(publish_scene(*node, scene, reason)) << reason;
    node->reset();
    EXPECT_EQ(onset(render(*node, input), 0, K_DIRECT_WINDOW, K_DIRECT_ONSET_FRACTION), compensated);
}

class Medium_RoomNodeInterauralDelay : public ::testing::TestWithParam<int>
{
};

TEST_P(Medium_RoomNodeInterauralDelay, RoomNodeReproducesWoodworthsInterauralDelayOnTheFallback)
{
    // The grid holds these azimuths, so the pair is not interpolated; the lag is read to a sample.
    const double theta = GetParam() * K_PI / 180.0;
    RoomScene scene = centred_dead_room();
    scene.listener_position = {8.0, 6.0, 6.0};
    scene.listener_yaw_rad = K_PI;
    scene.source_position = {8.0 - K_EARS_DISTANCE_M * std::cos(theta),
                             6.0 - K_EARS_DISTANCE_M * std::sin(theta), 6.0};
    scene.source_yaw_rad = theta;
    const std::array<std::vector<float>, 2> ears =
        render_both(*prepared_node(scene, K_LISTENER_EARS), impulse(K_LAG_BLOCKS * K_BLOCK));
    const double lag = interaural_lag(ears[0], ears[1], 0, K_LAG_WINDOW_FRAMES, K_MAX_LAG_FRAMES);
    EXPECT_NEAR(lag / K_SAMPLE_RATE, SushiDSP::room::SphericalHeadHrtf::interaural_time_difference(theta),
                1.0 / K_SAMPLE_RATE);
}

INSTANTIATE_TEST_SUITE_P(Degrees, Medium_RoomNodeInterauralDelay, ::testing::Values(30, 60, 90));

TEST(Medium_RoomNodeHrtfSource, RoomNodeStartsOnTheSphericalHead)
{
    SushiDSP::room::RoomNode node;
    node.prepare(K_INFO);
    EXPECT_EQ(node.hrtf_source(), SushiDSP::room::HrtfSource::SphericalHead);
}

TEST(Medium_RoomNodeHrtfSource, RoomNodeReportsAnInstalledSetAsMeasured)
{
    SushiDSP::room::RoomNode node;
    node.prepare(K_INFO);
    SushiDSP::room::HrtfSet fixture;
    std::string error;
    ASSERT_TRUE(load_fixture(fixture, error)) << error;
    ASSERT_EQ(fixture.direction_count(), K_FIXTURE_DIRECTIONS);
    EXPECT_EQ(node.set_hrtf(std::move(fixture)), SushiDSP::room::HrtfStatus::Installed);
    EXPECT_EQ(node.hrtf_source(), SushiDSP::room::HrtfSource::Measured);
}

TEST(Medium_RoomNodeHrtfSource, RoomNodeKeepsRenderingFiniteAudioThroughAnInstalledSet)
{
    std::unique_ptr<SushiDSP::room::RoomNode> node =
        prepared_node(centred_dead_room(), K_LISTENER_EARS);
    SushiDSP::room::HrtfSet fixture;
    std::string error;
    ASSERT_TRUE(load_fixture(fixture, error)) << error;
    ASSERT_EQ(node->set_hrtf(std::move(fixture)), SushiDSP::room::HrtfStatus::Installed);
    EXPECT_TRUE(all_finite(render(*node, impulse(K_HRTF_RENDER_FRAMES))));
}

TEST(Medium_RoomNodeHrtfSource, RoomNodeRestoresTheSphericalHeadFromAnEmptySet)
{
    SushiDSP::room::RoomNode node;
    node.prepare(K_INFO);
    SushiDSP::room::HrtfSet fixture;
    std::string error;
    ASSERT_TRUE(load_fixture(fixture, error)) << error;
    ASSERT_EQ(node.set_hrtf(std::move(fixture)), SushiDSP::room::HrtfStatus::Installed);
    ASSERT_EQ(node.set_hrtf(SushiDSP::room::HrtfSet{}), SushiDSP::room::HrtfStatus::Installed);
    EXPECT_EQ(node.hrtf_source(), SushiDSP::room::HrtfSource::SphericalHead);
}

TEST(Medium_RoomNodeHrtfSource, RoomNodeRefusesASetWithoutAThirdOrderDecoderWhole)
{
    SushiDSP::room::RoomNode node;
    node.prepare(K_INFO);
    SushiDSP::room::HrtfSet bare;
    std::string error;
    ASSERT_TRUE(load_fixture(bare, error)) << error;
    bare.set_decoder(1, std::vector<float>(K_FIRST_ORDER_DECODER_SIZE));
    EXPECT_EQ(node.set_hrtf(std::move(bare)), SushiDSP::room::HrtfStatus::DecoderRefused);
    EXPECT_EQ(node.hrtf_source(), SushiDSP::room::HrtfSource::SphericalHead);
}

TEST(Medium_RoomNodeTail, RoomNodeTailIsCoherentInTheLowOctave)
{
    // A window 1024 frames after the taps end holds the tail alone.
    ASSERT_GT(energy(tail_render()[0], tail_begin(), tail_begin() + K_TAIL_WINDOW_FRAMES), 0.0);
    const double low = tail_iacc(125.0);
    RecordProperty("iacc_125_hz", low);
    EXPECT_GT(low, K_LOW_IACC_FLOOR);
}

TEST(Medium_RoomNodeTail, RoomNodeTailIsIncoherentInTheHighOctave)
{
    ASSERT_GT(energy(tail_render()[0], tail_begin(), tail_begin() + K_TAIL_WINDOW_FRAMES), 0.0);
    const double high = tail_iacc(4000.0);
    RecordProperty("iacc_4_khz", high);
    EXPECT_LT(high, K_HIGH_IACC_CEILING);
}
