/**************************************************************************/
/* room_node_rig.hpp                                                      */
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
 * @file room_node_rig.hpp
 * @brief Shares the scenes, the node builders and the render and spectrum helpers of the
 * RoomNode tests.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/core/audio_node.hpp>
#include <SushiDSP/foundation/math/fft.hpp>
#include <SushiDSP/nodes/room/room_arrangement.hpp>
#include <SushiDSP/nodes/room/room_node.hpp>
#include <SushiDSP/nodes/room_scenes/room_fixtures.hpp>

namespace SushiDSP::test::room_node_rig
{
    /// @brief Holds pi.
    inline constexpr double K_PI = 3.14159265358979323846;

    /// @brief Holds the sample rate every node of the RoomNode tests runs at.
    inline constexpr double K_SAMPLE_RATE = 48000.0;

    /// @brief Holds the block length every node of the RoomNode tests renders in.
    inline constexpr std::uint32_t K_BLOCK = 256;

    /// @brief Holds the frames one metre of path takes at K_SAMPLE_RATE and 343 m/s.
    inline constexpr double K_FRAMES_PER_METRE = K_SAMPLE_RATE / 343.0;

    /// @brief Holds the PrepareInfo every node of the RoomNode tests is prepared with.
    inline constexpr SushiDSP::PrepareInfo K_INFO{K_SAMPLE_RATE, K_BLOCK, 1};

    /// @brief Holds the listener parameter's value for the ears.
    inline constexpr float K_LISTENER_EARS = 0.0f;

    /// @brief Holds the listener parameter's value for the microphone.
    inline constexpr float K_LISTENER_MICROPHONE = 1.0f;

    /// @brief Holds the direct_delay parameter's value for the compensated choice.
    inline constexpr float K_DIRECT_COMPENSATED = 0.0f;

    /// @brief Holds the direct_delay parameter's value for the physical choice.
    inline constexpr float K_DIRECT_PHYSICAL = 1.0f;

    /// @brief Holds the decoder's partition at K_SAMPLE_RATE, the frames every ears path waits.
    inline constexpr std::uint32_t K_ALIGN_FRAMES = 128;

    /// @brief Holds the identity the one microphone of these nodes is placed under.
    inline constexpr std::uint64_t K_MICROPHONE_ID = 1;

    /// @brief Holds the length of the direct-path windows of the dead room, in frames.
    inline constexpr std::size_t K_DIRECT_WINDOW = 1500;

    /// @brief Holds the blocks that cover K_DIRECT_WINDOW: six of 256 frames are 1536.
    inline constexpr std::size_t K_DIRECT_WINDOW_BLOCKS = 6;

    /// @brief Holds the transform length of the direct-path spectra.
    inline constexpr std::uint32_t K_DIRECT_FFT = 65536;

    /// @brief Names an edit applied to a node between two blocks on the control thread.
    using NodeEdit = std::function<void(SushiDSP::room::RoomNode&)>;

    /// @brief Returns the frames the ears' direct sound lands at, compensated: the alignment
    /// delay plus the fraction the rounded propagation delay leaves of a path of some metres.
    inline double compensated_direct_frames(double metres)
    {
        const double frames = metres * K_FRAMES_PER_METRE;
        return static_cast<double>(K_ALIGN_FRAMES) + frames - std::round(frames);
    }

    /// @brief Returns the garage with the source at (1.5, 2.0, 0.5) and an omni microphone at
    /// (4.0, 2.0, 1.2), the listener spot of test_image_source_solver.cpp's example scene.
    inline SushiDSP::room::RoomScene microphone_scene()
    {
        SushiDSP::room::RoomScene scene = SushiDSP::test::garage_scene();
        scene.source_position = {1.5, 2.0, 0.5};
        scene.microphone_position = {4.0, 2.0, 1.2};
        scene.microphone_pattern = SushiDSP::room::PolarPattern::Omni;
        return scene;
    }

    /// @brief Returns the garage with the cabinet near one corner at (0.4, 0.5, 0.5) and the
    /// listener and an omni microphone together near the opposite corner at (5.4, 3.4, 2.0):
    /// psi at the cabinet times psi at the listener is 0.53 to 0.93 in magnitude for each of
    /// the first eight modes, so every one of them is driven and heard.
    inline SushiDSP::room::RoomScene wave_scene()
    {
        SushiDSP::room::RoomScene scene = SushiDSP::test::garage_scene();
        scene.source_position = {0.4, 0.5, 0.5};
        scene.listener_position = {5.4, 3.4, 2.0};
        scene.microphone_position = {5.4, 3.4, 2.0};
        scene.microphone_pattern = SushiDSP::room::PolarPattern::Omni;
        return scene;
    }

    /// @brief Returns a 12 x 12 x 12 m room of heavy carpet with four faces open: Eyring's
    /// mid-band RT60 is 0.21 s and f_s = 2000 sqrt(0.21 / 1728) = 22 Hz, under the 40 Hz at
    /// which ModalSolver switches the layer off.
    inline SushiDSP::room::RoomScene dead_room_scene()
    {
        using SushiDSP::room::MaterialId;
        using SushiDSP::room::Surface;
        SushiDSP::room::RoomScene scene;
        scene.dimensions = {12.0, 12.0, 12.0};
        scene.surfaces.fill(MaterialId::HeavyCarpetOnConcrete);
        scene.scattering.fill(SushiDSP::room::Scattering::Flat);
        scene.patches[0] = {Surface::MinusX, MaterialId::OpenDoorway, 144.0};
        scene.patches[1] = {Surface::PlusX, MaterialId::OpenDoorway, 144.0};
        scene.patches[2] = {Surface::MinusY, MaterialId::OpenDoorway, 144.0};
        scene.patches[3] = {Surface::PlusY, MaterialId::OpenDoorway, 144.0};
        scene.patch_count = 4;
        scene.source_position = {3.0, 4.0, 1.0};
        scene.listener_position = {7.0, 5.0, 1.5};
        scene.microphone_position = {7.0, 5.0, 1.5};
        scene.microphone_pattern = SushiDSP::room::PolarPattern::Omni;
        return scene;
    }

    /// @brief Returns the 12 m dead room with the cabinet at its centre facing +x, so the
    /// nearest reflection, off the floor or the ceiling, arrives at least 12 m after the direct
    /// path and a window of 1500 frames holds the direct path alone for either listener: the
    /// ears' floor image lands at 1703 - 280 + 128 = 1551 with the propagation compensated.
    inline SushiDSP::room::RoomScene centred_dead_room()
    {
        SushiDSP::room::RoomScene scene = dead_room_scene();
        scene.source_position = {6.0, 6.0, 6.0};
        scene.listener_position = {8.0, 6.0, 6.0};
        scene.microphone_position = {9.0, 6.0, 6.0};
        scene.microphone_yaw_rad = K_PI;
        scene.microphone_pattern = SushiDSP::room::PolarPattern::Cardioid;
        return scene;
    }

    /// @brief Returns a 3.0 x 2.5 x 2.4 m unpainted concrete room, whose Eyring f_s of 877 Hz
    /// has about 1250 modes under it, past K_MAX_MODES.
    inline SushiDSP::room::RoomScene small_concrete_room()
    {
        SushiDSP::room::RoomScene scene;
        scene.dimensions = {3.0, 2.5, 2.4};
        scene.surfaces.fill(SushiDSP::room::MaterialId::ConcreteUnpainted);
        scene.scattering.fill(SushiDSP::room::Scattering::Flat);
        scene.patch_count = 0;
        scene.source_position = {0.3, 0.3, 0.3};
        scene.listener_position = {1.5, 1.25, 1.2};
        scene.microphone_position = {1.5, 1.25, 1.2};
        scene.microphone_pattern = SushiDSP::room::PolarPattern::Omni;
        return scene;
    }

    /// @brief Returns a cabinet small enough to act as a point: one 1 cm cone that never breaks
    /// up, so its seven elements stand within a centimetre and the far-field pattern is flat.
    inline SushiDSP::room::CabinetGeometry point_cabinet()
    {
        return SushiDSP::room::CabinetGeometry{0.01, 0.0, 0.05, 20000.0, 1, 1, 0.05, 0.05};
    }

    /// @brief Returns the microphone a scene describes, as an arrangement holds it.
    inline SushiDSP::Room::MicrophonePlacement microphone_of(const SushiDSP::room::RoomScene& scene)
    {
        SushiDSP::Room::MicrophonePlacement microphone;
        microphone.id = K_MICROPHONE_ID;
        microphone.pose = {scene.microphone_position, scene.microphone_yaw_rad};
        microphone.pattern = scene.microphone_pattern;
        microphone.proximity = scene.microphone_proximity;
        return microphone;
    }

    /// @brief Publishes one scene to a node. A node that holds the microphone of these tests has
    /// it moved to the scene's microphone pose in the same publish.
    inline bool publish_scene(SushiDSP::room::RoomNode& node,
                              const SushiDSP::room::RoomScene& scene, std::string& reason)
    {
        SushiDSP::Room::Arrangement arrangement = node.arrangement();
        if (arrangement.microphones().empty())
        {
            return node.set_scene(scene, reason);
        }
        if (!scene.is_valid(reason))
        {
            return false;
        }
        arrangement.set_shell(scene);
        arrangement.set_listener({scene.listener_position, scene.listener_yaw_rad});
        arrangement.move_source(arrangement.sources().front().id,
                                {scene.source_position, scene.source_yaw_rad});
        arrangement.update_microphone(microphone_of(scene));
        return node.set_arrangement(arrangement, reason);
    }

    /// @brief Returns a prepared node rendering one scene for one listener: K_LISTENER_EARS reads
    /// the listener's port, K_LISTENER_MICROPHONE places the scene's microphone and reads its
    /// port. The microphone's node takes the scene and the microphone in one publish, where the
    /// scene and then the microphone would solve the room twice.
    inline std::unique_ptr<SushiDSP::room::RoomNode>
    prepared_node(const SushiDSP::room::RoomScene& scene, float listener)
    {
        auto node = std::make_unique<SushiDSP::room::RoomNode>();
        node->prepare(K_INFO);
        std::string reason;
        if (listener == K_LISTENER_MICROPHONE)
        {
            SushiDSP::Room::Arrangement arrangement =
                SushiDSP::Room::Arrangement::from_scene(scene);
            EXPECT_TRUE(arrangement.add_microphone(microphone_of(scene), reason)) << reason;
            EXPECT_TRUE(node->set_arrangement(arrangement, reason)) << reason;
            // The node renders the set in place from zero state, as a fresh node would.
            node->reset();
        }
        else
        {
            EXPECT_TRUE(node->set_scene(scene, reason)) << reason;
        }
        return node;
    }

    /// @brief Returns the edit that publishes one scene.
    inline NodeEdit scene_edit(const SushiDSP::room::RoomScene& scene)
    {
        return [&scene](SushiDSP::room::RoomNode& node)
        {
            std::string reason;
            EXPECT_TRUE(publish_scene(node, scene, reason)) << reason;
        };
    }

    /// @brief Runs one input through a node block by block, applying the edit before each
    /// block listed in edit_blocks, and returns both output channels. A node that holds the
    /// microphone of these tests is heard at the microphone's port, on both channels, with the
    /// port's alignment delay taken back out, so a frame index means what it means at the
    /// capsule; the last K_ALIGN_FRAMES frames of such a render are zero.
    inline std::array<std::vector<float>, 2>
    render_both(SushiDSP::room::RoomNode& node, const std::vector<float>& input,
                const std::vector<std::uint32_t>& edit_blocks = {}, const NodeEdit& edit = {})
    {
        std::array<std::vector<float>, 2> output = {std::vector<float>(input.size(), 0.0f),
                                                    std::vector<float>(input.size(), 0.0f)};
        std::vector<float> left(K_BLOCK, 0.0f);
        std::vector<float> right(K_BLOCK, 0.0f);
        float* outputs[2] = {left.data(), right.data()};
        const bool at_microphone = !node.arrangement().microphones().empty();
        constexpr std::uint32_t K_PORTS = SushiDSP::room::RoomNode::K_OUTPUT_PORTS;
        std::vector<std::vector<float>> microphone_blocks(K_PORTS - 1,
                                                          std::vector<float>(K_BLOCK, 0.0f));
        std::array<float*, K_PORTS - 1> microphone_channels{};
        std::array<float* const*, K_PORTS> port_table{};
        port_table[0] = outputs;
        for (std::uint32_t port = 1; port < K_PORTS; ++port)
        {
            microphone_channels[port - 1] = microphone_blocks[port - 1].data();
            port_table[port] = &microphone_channels[port - 1];
        }
        for (std::size_t offset = 0; offset + K_BLOCK <= input.size(); offset += K_BLOCK)
        {
            const std::uint32_t block = static_cast<std::uint32_t>(offset / K_BLOCK);
            if (edit && std::find(edit_blocks.begin(), edit_blocks.end(), block) != edit_blocks.end())
            {
                edit(node);
            }
            const float* inputs[1] = {input.data() + offset};
            SushiDSP::ProcessContext context{outputs, inputs, K_BLOCK, 0.0, port_table.data()};
            node.process(context);
            if (!at_microphone)
            {
                std::memcpy(output[0].data() + offset, left.data(), K_BLOCK * sizeof(float));
                std::memcpy(output[1].data() + offset, right.data(), K_BLOCK * sizeof(float));
                continue;
            }
            for (std::uint32_t frame = 0; frame < K_BLOCK; ++frame)
            {
                const std::size_t absolute = offset + frame;
                if (absolute >= K_ALIGN_FRAMES)
                {
                    output[0][absolute - K_ALIGN_FRAMES] = microphone_blocks[0][frame];
                    output[1][absolute - K_ALIGN_FRAMES] = microphone_blocks[0][frame];
                }
            }
        }
        return output;
    }

    /// @brief Runs one input through a node as render_both does and returns the left channel.
    inline std::vector<float> render(SushiDSP::room::RoomNode& node,
                                     const std::vector<float>& input,
                                     const std::vector<std::uint32_t>& edit_blocks = {},
                                     const NodeEdit& edit = {})
    {
        return render_both(node, input, edit_blocks, edit)[0];
    }

    /// @brief Returns a unit impulse of the given length.
    inline std::vector<float> impulse(std::size_t frames)
    {
        std::vector<float> samples(frames, 0.0f);
        samples[0] = 1.0f;
        return samples;
    }

    /// @brief Returns a full-scale sine of the given frequency and length.
    inline std::vector<float> sine(double frequency_hz, std::size_t frames)
    {
        std::vector<float> samples(frames);
        for (std::size_t frame = 0; frame < frames; ++frame)
        {
            samples[frame] = static_cast<float>(
                std::sin(2.0 * K_PI * frequency_hz * static_cast<double>(frame) / K_SAMPLE_RATE));
        }
        return samples;
    }

    /// @brief Returns a 1 kHz full-scale tone of the given length.
    inline std::vector<float> tone(std::size_t frames)
    {
        return sine(1000.0, frames);
    }

    /// @brief Returns whether every sample of a signal is finite.
    inline bool all_finite(const std::vector<float>& signal)
    {
        return std::all_of(signal.begin(), signal.end(),
                           [](float sample) { return std::isfinite(sample); });
    }

    /// @brief Returns the arriving set's weight the node uses at one frame of its fade.
    inline double fade_weight(std::size_t frame)
    {
        const double phase = static_cast<double>(frame + 1) /
                             static_cast<double>(SushiDSP::room::RoomNode::K_CROSSFADE_FRAMES);
        return 0.5 * (1.0 - std::cos(K_PI * phase));
    }

    /// @brief Returns the largest |a[n] - b[n]| over [begin, end).
    inline double peak_difference(const std::vector<float>& a, const std::vector<float>& b,
                                  std::size_t begin, std::size_t end)
    {
        double worst = 0.0;
        for (std::size_t frame = begin; frame < end; ++frame)
        {
            worst = std::max(worst, static_cast<double>(std::fabs(a[frame] - b[frame])));
        }
        return worst;
    }

    /// @brief Returns the largest magnitude of a signal over [begin, end).
    inline double peak_of(const std::vector<float>& signal, std::size_t begin, std::size_t end)
    {
        double peak = 0.0;
        for (std::size_t frame = begin; frame < end; ++frame)
        {
            peak = std::max(peak, static_cast<double>(std::fabs(signal[frame])));
        }
        return peak;
    }

    /// @brief Returns 20 log10 of a linear value.
    inline double to_decibels(double value)
    {
        return 20.0 * std::log10(std::max(value, 1e-30));
    }

    /// @brief Returns the energy of a signal over [begin, end).
    inline double energy(const std::vector<float>& signal, std::size_t begin, std::size_t end)
    {
        double sum = 0.0;
        for (std::size_t frame = begin; frame < end; ++frame)
        {
            sum += static_cast<double>(signal[frame]) * signal[frame];
        }
        return sum;
    }

    /// @brief Returns the power spectrum in decibels, bins 0 to size / 2, of one signal.
    inline std::vector<double> power_spectrum_db(const std::vector<float>& signal,
                                                 std::uint32_t size)
    {
        SushiDSP::Fft fft(size);
        std::vector<float> real(size);
        std::vector<float> imaginary(size);
        fft.forward(signal.data(), real.data(), imaginary.data());
        std::vector<double> power(size / 2 + 1);
        for (std::size_t bin = 0; bin < power.size(); ++bin)
        {
            const double bin_energy = static_cast<double>(real[bin]) * real[bin] +
                                      static_cast<double>(imaginary[bin]) * imaginary[bin];
            power[bin] = 10.0 * std::log10(std::max(bin_energy, 1e-30));
        }
        return power;
    }

    /// @brief Returns the power spectrum of one window of a signal, zero-padded to K_DIRECT_FFT.
    inline std::vector<double> windowed_power_db(const std::vector<float>& signal,
                                                 std::size_t begin, std::size_t end)
    {
        std::vector<float> window(K_DIRECT_FFT, 0.0f);
        std::copy(signal.begin() + static_cast<std::ptrdiff_t>(begin),
                  signal.begin() + static_cast<std::ptrdiff_t>(end), window.begin());
        return power_spectrum_db(window, K_DIRECT_FFT);
    }

    /// @brief Returns 10 log10 of the mean power per hertz of a K_DIRECT_FFT spectrum over the
    /// third octave centred on one frequency.
    inline double third_octave_db(const std::vector<double>& power_db, double centre_hz)
    {
        const double hz_per_bin = K_SAMPLE_RATE / static_cast<double>(K_DIRECT_FFT);
        const double edge = std::pow(2.0, 1.0 / 6.0);
        const std::size_t first =
            static_cast<std::size_t>(std::ceil(centre_hz / edge / hz_per_bin));
        const std::size_t last = static_cast<std::size_t>(std::ceil(centre_hz * edge / hz_per_bin));
        double sum = 0.0;
        for (std::size_t bin = first; bin < last; ++bin)
        {
            sum += std::pow(10.0, power_db[bin] / 10.0);
        }
        return 10.0 * std::log10(sum / static_cast<double>(last - first));
    }

    /// @brief Returns the first frame of [begin, end) whose magnitude exceeds a fraction of
    /// the window's peak, or -1 when the window is silent.
    inline long onset(const std::vector<float>& signal, std::size_t begin, std::size_t end,
                      double fraction)
    {
        const double peak = peak_of(signal, begin, end);
        if (peak <= 0.0)
        {
            return -1;
        }
        for (std::size_t frame = begin; frame < end; ++frame)
        {
            if (std::fabs(signal[frame]) > fraction * peak)
            {
                return static_cast<long>(frame);
            }
        }
        return -1;
    }

    /// @brief Returns the power spectrum of the direct-path window of a scene for one listener:
    /// the window opens the render, so a render of K_DIRECT_WINDOW_BLOCKS blocks holds it.
    inline std::vector<double> direct_power_db(const SushiDSP::room::RoomScene& scene,
                                               float listener)
    {
        std::unique_ptr<SushiDSP::room::RoomNode> node = prepared_node(scene, listener);
        return windowed_power_db(
            render(*node, impulse(K_DIRECT_WINDOW_BLOCKS * K_BLOCK)), 0, K_DIRECT_WINDOW);
    }
    /// @brief Holds what one publish measures against the raised-cosine blend of two steady
    /// renders.
    struct FadeMeasure
    {
            /// @brief Stores the moving render's largest sample-to-sample step over the fade,
            /// divided by the reference's own.
            double step_ratio;

            /// @brief Stores the largest difference between the moving render and the reference
            /// from the swap to one block past the fade.
            double residual;

            /// @brief Stores the reference's largest magnitude over the same frames.
            double peak;

            /// @brief Stores the moving render's energy over the four blocks after the fade
            /// against the arriving scene's steady render, in decibels.
            double settled_db;
    };

    /// @brief Measures a render that takes a publish at one frame against the blend of the two
    /// steady renders: the weight is 0 before the swap, fade_weight over the fade and 1 after.
    /// The renders must run four blocks past the fade's end.
    inline FadeMeasure measure_fade(const std::vector<float>& steady_a,
                                    const std::vector<float>& steady_b,
                                    const std::vector<float>& moving, std::size_t swap)
    {
        const std::size_t fade_end = swap + SushiDSP::room::RoomNode::K_CROSSFADE_FRAMES;
        const auto reference = [&](std::size_t frame)
        {
            const double weight =
                frame >= fade_end ? 1.0 : (frame >= swap ? fade_weight(frame - swap) : 0.0);
            return (1.0 - weight) * steady_a[frame] + weight * steady_b[frame];
        };
        double own_step = 0.0;
        double moving_step = 0.0;
        double peak = 0.0;
        double residual = 0.0;
        for (std::size_t frame = swap - K_BLOCK; frame < fade_end + K_BLOCK; ++frame)
        {
            own_step = std::max(own_step, std::fabs(reference(frame) - reference(frame - 1)));
            moving_step = std::max(
                moving_step, static_cast<double>(std::fabs(moving[frame] - moving[frame - 1])));
            if (frame >= swap)
            {
                peak = std::max(peak, std::fabs(reference(frame)));
                residual = std::max(residual, std::fabs(moving[frame] - reference(frame)));
            }
        }
        const double moving_energy = energy(moving, fade_end, fade_end + 4 * K_BLOCK);
        const double steady_energy = energy(steady_b, fade_end, fade_end + 4 * K_BLOCK);
        return {moving_step / own_step, residual, peak,
                10.0 * std::log10(moving_energy / steady_energy)};
    }
} // namespace SushiDSP::test::room_node_rig
