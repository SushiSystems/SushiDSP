/**************************************************************************/
/* test_behaviour.cpp                                                     */
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
 * @file test_behaviour.cpp
 * @brief Tests for RoomNode: its ports, its tap placement, its crossfade, its swap, its standing
 * waves, its seam and its mix.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <SushiDSP/nodes/room_scenes/room_fixtures.hpp>
#include <SushiDSP/foundation/core/audio_node.hpp>
#include <SushiDSP/foundation/math/fft.hpp>
#include <SushiDSP/nodes/room/ambisonic_encoder.hpp>
#include <SushiDSP/nodes/room/binaural_decoder.hpp>
#include <SushiDSP/nodes/room/cabinet_directivity.hpp>
#include <SushiDSP/nodes/room/diffuse_field.hpp>
#include <SushiDSP/nodes/room/hrtf_set.hpp>
#include <SushiDSP/nodes/room/hrtf_set_loader.hpp>
#include <SushiDSP/nodes/room/early_reflection_renderer.hpp>
#include <SushiDSP/nodes/room/eyring_decay.hpp>
#include <SushiDSP/nodes/room/image_source_solver.hpp>
#include <SushiDSP/nodes/room/air_absorption.hpp>
#include <SushiDSP/nodes/room/late_reverb.hpp>
#include <SushiDSP/nodes/room/microphone_model.hpp>
#include <SushiDSP/nodes/room/modal_solver.hpp>
#include <SushiDSP/nodes/room/room_node.hpp>
#include <SushiDSP/nodes/room/room_arrangement.hpp>
#include <SushiDSP/nodes/room/room_render_set.hpp>
#include <SushiDSP/nodes/room/spherical_head_hrtf.hpp>

namespace
{
    /// @brief Holds pi.
    constexpr double K_PI = 3.14159265358979323846;

    /// @brief Holds the sample rate every node in this file runs at.
    constexpr double K_SAMPLE_RATE = 48000.0;

    /// @brief Holds the block length every node in this file renders in.
    constexpr std::uint32_t K_BLOCK = 256;

    /// @brief Holds the frames one metre of path takes at K_SAMPLE_RATE and 343 m/s.
    constexpr double K_FRAMES_PER_METRE = K_SAMPLE_RATE / 343.0;

    /// @brief Holds the PrepareInfo every node in this file is prepared with.
    constexpr SushiDSP::PrepareInfo K_INFO{K_SAMPLE_RATE, K_BLOCK, 1};

    /// @brief Holds the listener parameter's value for the ears.
    constexpr float K_LISTENER_EARS = 0.0f;

    /// @brief Holds the listener parameter's value for the microphone.
    constexpr float K_LISTENER_MICROPHONE = 1.0f;

    /// @brief Holds the direct_delay parameter's value for the compensated choice.
    constexpr float K_DIRECT_COMPENSATED = 0.0f;

    /// @brief Holds the direct_delay parameter's value for the physical choice.
    constexpr float K_DIRECT_PHYSICAL = 1.0f;

    /// @brief Holds the decoder's partition at K_SAMPLE_RATE, the frames every ears path waits.
    constexpr std::uint32_t K_ALIGN_FRAMES = 128;

    /// @brief Returns the frames the ears' direct sound lands at, compensated: the alignment
    /// delay plus the fraction the rounded propagation delay leaves of a path of some metres.
    double compensated_direct_frames(double metres)
    {
        const double frames = metres * K_FRAMES_PER_METRE;
        return static_cast<double>(K_ALIGN_FRAMES) + frames - std::round(frames);
    }

    /// @brief Names an edit applied to a node between two blocks on the control thread.
    using NodeEdit = std::function<void(SushiDSP::room::RoomNode&)>;

    /// @brief Returns the garage with the source at (1.5, 2.0, 0.5) and an omni microphone at
    /// (4.0, 2.0, 1.2), the listener spot of test_image_source_solver.cpp's example scene.
    SushiDSP::room::RoomScene microphone_scene()
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
    SushiDSP::room::RoomScene wave_scene()
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
    SushiDSP::room::RoomScene dead_room_scene()
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

    /// @brief Returns a 3.0 x 2.5 x 2.4 m unpainted concrete room, whose Eyring f_s of 877 Hz
    /// has about 1250 modes under it, past K_MAX_MODES.
    SushiDSP::room::RoomScene small_concrete_room()
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

    /// @brief Holds the length of the impulse renders the wave tests transform.
    constexpr std::uint32_t K_WAVE_FFT = 262144;

    /// @brief Returns the power spectrum in decibels, bins 0 to size / 2, of one signal.
    std::vector<double> power_spectrum_db(const std::vector<float>& signal, std::uint32_t size)
    {
        SushiDSP::Fft fft(size);
        std::vector<float> real(size);
        std::vector<float> imaginary(size);
        fft.forward(signal.data(), real.data(), imaginary.data());
        std::vector<double> power(size / 2 + 1);
        for (std::size_t bin = 0; bin < power.size(); ++bin)
        {
            const double energy = static_cast<double>(real[bin]) * real[bin] +
                                  static_cast<double>(imaginary[bin]) * imaginary[bin];
            power[bin] = 10.0 * std::log10(std::max(energy, 1e-30));
        }
        return power;
    }

    /// @brief Returns the frequency of one bin of a K_WAVE_FFT transform.
    double bin_hz(double bin)
    {
        return bin * K_SAMPLE_RATE / static_cast<double>(K_WAVE_FFT);
    }

    /// @brief Returns 10 log10 of the mean power per hertz between two frequencies.
    double band_density_db(const std::vector<double>& power_db, double lower_hz, double upper_hz)
    {
        const std::size_t first = static_cast<std::size_t>(std::ceil(lower_hz / bin_hz(1.0)));
        const std::size_t last = static_cast<std::size_t>(std::ceil(upper_hz / bin_hz(1.0)));
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
    std::vector<SpectralPeak> find_peaks(const std::vector<double>& power_db, double lower_hz,
                                         double upper_hz, double prominence_db)
    {
        const std::size_t first = static_cast<std::size_t>(std::ceil(lower_hz / bin_hz(1.0)));
        const std::size_t last = static_cast<std::size_t>(std::floor(upper_hz / bin_hz(1.0)));
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
            peaks.push_back(
                {bin_hz(static_cast<double>(bin) + offset), bin_hz(high_edge - low_edge)});
        }
        return peaks;
    }

    /// @brief Holds the identity the one microphone of this file's nodes is placed under.
    constexpr std::uint64_t K_MICROPHONE_ID = 1;

    /// @brief Returns the microphone a scene describes, as an arrangement holds it.
    SushiDSP::Room::MicrophonePlacement microphone_of(const SushiDSP::room::RoomScene& scene)
    {
        SushiDSP::Room::MicrophonePlacement microphone;
        microphone.id = K_MICROPHONE_ID;
        microphone.pose = {scene.microphone_position, scene.microphone_yaw_rad};
        microphone.pattern = scene.microphone_pattern;
        microphone.proximity = scene.microphone_proximity;
        return microphone;
    }

    /// @brief Publishes one scene to a node. A node that holds this file's microphone has it
    /// moved to the scene's microphone pose in the same publish.
    bool publish_scene(SushiDSP::room::RoomNode& node, const SushiDSP::room::RoomScene& scene,
                       std::string& reason)
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
    /// the listener's port, K_LISTENER_MICROPHONE places the scene's microphone and reads its port.
    std::unique_ptr<SushiDSP::room::RoomNode> prepared_node(const SushiDSP::room::RoomScene& scene,
                                                            float listener)
    {
        auto node = std::make_unique<SushiDSP::room::RoomNode>();
        node->prepare(K_INFO);
        std::string reason;
        EXPECT_TRUE(node->set_scene(scene, reason)) << reason;
        if (listener == K_LISTENER_MICROPHONE)
        {
            SushiDSP::Room::Arrangement arrangement = node->arrangement();
            EXPECT_TRUE(arrangement.add_microphone(microphone_of(scene), reason)) << reason;
            EXPECT_TRUE(node->set_arrangement(arrangement, reason)) << reason;
            // The node renders the set in place from zero state, as a fresh node would.
            node->reset();
        }
        return node;
    }

    /// @brief Returns the edit that publishes one scene.
    NodeEdit scene_edit(const SushiDSP::room::RoomScene& scene)
    {
        return [&scene](SushiDSP::room::RoomNode& node)
        {
            std::string reason;
            EXPECT_TRUE(publish_scene(node, scene, reason)) << reason;
        };
    }

    /// @brief Runs one input through a node block by block, applying the edit before each
    /// block listed in edit_blocks, and returns both output channels. A node that holds this
    /// file's microphone is heard at the microphone's port, on both channels, with the port's
    /// alignment delay taken back out, so a frame index means what it means at the capsule; the
    /// last K_ALIGN_FRAMES frames of such a render are zero.
    std::array<std::vector<float>, 2>
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
            for (std::uint32_t edited : edit_blocks)
            {
                if (edited == block && edit)
                {
                    edit(node);
                }
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
    std::vector<float> render(SushiDSP::room::RoomNode& node, const std::vector<float>& input,
                              const std::vector<std::uint32_t>& edit_blocks = {},
                              const NodeEdit& edit = {})
    {
        return render_both(node, input, edit_blocks, edit)[0];
    }

    /// @brief Returns a 1 kHz full-scale tone of the given length.
    std::vector<float> tone(std::size_t frames)
    {
        std::vector<float> samples(frames);
        for (std::size_t frame = 0; frame < frames; ++frame)
        {
            samples[frame] = static_cast<float>(
                std::sin(2.0 * K_PI * 1000.0 * static_cast<double>(frame) / K_SAMPLE_RATE));
        }
        return samples;
    }

    /// @brief Returns the impulse response of a scene heard by the ears, K_WAVE_FFT frames of
    /// the left output.
    std::vector<float> wave_response(const SushiDSP::room::RoomScene& scene)
    {
        std::unique_ptr<SushiDSP::room::RoomNode> node = prepared_node(scene, K_LISTENER_EARS);
        std::vector<float> impulse(K_WAVE_FFT, 0.0f);
        impulse[0] = 1.0f;
        return render(*node, impulse);
    }

    /// @brief Returns the arriving set's weight the node uses at one frame of its fade.
    double fade_weight(std::size_t frame)
    {
        const double phase = static_cast<double>(frame + 1) /
                             static_cast<double>(SushiDSP::room::RoomNode::K_CROSSFADE_FRAMES);
        return 0.5 * (1.0 - std::cos(K_PI * phase));
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

    /// @brief Returns the largest |a[n] - b[n]| over [begin, end).
    double peak_difference(const std::vector<float>& a, const std::vector<float>& b,
                           std::size_t begin, std::size_t end)
    {
        double worst = 0.0;
        for (std::size_t frame = begin; frame < end; ++frame)
        {
            worst = std::max(worst, static_cast<double>(std::fabs(a[frame] - b[frame])));
        }
        return worst;
    }

    /// @brief Returns 20 log10 of a linear value.
    double to_decibels(double value)
    {
        return 20.0 * std::log10(std::max(value, 1e-30));
    }

    /// @brief Holds what one publish under the crossfade measures.
    struct CrossfadeMeasure
    {
            /// @brief Stores the moving render's largest sample-to-sample step over the fade,
            /// divided by the reference's own.
            double step_ratio;

            /// @brief Stores the largest difference between the moving render and the reference
            /// from the swap to one block past the fade.
            double residual;

            /// @brief Stores the reference's largest magnitude over the same frames.
            double peak;
    };

    /// @brief Renders a tone through three nodes: a and b steady, and a third that starts as
    /// a and takes the edit at block 200; the reference is the raised-cosine blend of the two
    /// steady renders over the node's fade, which starts lag frames after the edit: one
    /// partition when the ears take part, when the decoder's first partition of the arriving
    /// set comes out, and at once for the microphone alone.
    CrossfadeMeasure measure_crossfade(SushiDSP::room::RoomNode& steady_a,
                                       SushiDSP::room::RoomNode& steady_b,
                                       SushiDSP::room::RoomNode& moving, const NodeEdit& edit,
                                       std::size_t lag)
    {
        constexpr std::uint32_t K_SWAP_BLOCK = 200;
        constexpr std::size_t K_FRAMES = 400 * K_BLOCK;
        const std::vector<float> input = tone(K_FRAMES);

        const std::vector<float> render_a = render(steady_a, input);
        const std::vector<float> render_b = render(steady_b, input);
        const std::vector<float> render_x = render(moving, input, {K_SWAP_BLOCK}, edit);

        const std::size_t swap = static_cast<std::size_t>(K_SWAP_BLOCK) * K_BLOCK + lag;
        const std::size_t fade_end = swap + SushiDSP::room::RoomNode::K_CROSSFADE_FRAMES;
        std::vector<float> reference(K_FRAMES);
        for (std::size_t frame = 0; frame < K_FRAMES; ++frame)
        {
            double weight = 0.0;
            if (frame >= fade_end)
            {
                weight = 1.0;
            }
            else if (frame >= swap)
            {
                weight = fade_weight(frame - swap);
            }
            reference[frame] =
                static_cast<float>((1.0 - weight) * render_a[frame] + weight * render_b[frame]);
        }
        EXPECT_EQ(peak_difference(render_x, render_a, 0, swap), 0.0);

        double own_step = 0.0;
        double moving_step = 0.0;
        double peak = 0.0;
        for (std::size_t frame = swap - K_BLOCK; frame < fade_end + K_BLOCK; ++frame)
        {
            own_step = std::max(
                own_step, static_cast<double>(std::fabs(reference[frame] - reference[frame - 1])));
            moving_step = std::max(
                moving_step, static_cast<double>(std::fabs(render_x[frame] - render_x[frame - 1])));
            if (frame >= swap)
            {
                peak = std::max(peak, static_cast<double>(std::fabs(reference[frame])));
            }
        }
        return {moving_step / own_step,
                peak_difference(render_x, reference, swap, fade_end + K_BLOCK), peak};
    }

    /// @brief Measures the microphone listener's crossfade from scene a to scene b.
    CrossfadeMeasure measure_crossfade(const SushiDSP::room::RoomScene& scene_a,
                                       const SushiDSP::room::RoomScene& scene_b)
    {
        std::unique_ptr<SushiDSP::room::RoomNode> steady_a =
            prepared_node(scene_a, K_LISTENER_MICROPHONE);
        std::unique_ptr<SushiDSP::room::RoomNode> steady_b =
            prepared_node(scene_b, K_LISTENER_MICROPHONE);
        std::unique_ptr<SushiDSP::room::RoomNode> moving =
            prepared_node(scene_a, K_LISTENER_MICROPHONE);
        return measure_crossfade(*steady_a, *steady_b, *moving, scene_edit(scene_b), 0);
    }

    /// @brief Holds the transform length of the direct-path spectra.
    constexpr std::uint32_t K_DIRECT_FFT = 65536;

    /// @brief Returns the power spectrum of one window of a signal, zero-padded to K_DIRECT_FFT.
    std::vector<double> windowed_power_db(const std::vector<float>& signal, std::size_t begin,
                                          std::size_t end)
    {
        std::vector<float> window(K_DIRECT_FFT, 0.0f);
        std::copy(signal.begin() + static_cast<std::ptrdiff_t>(begin),
                  signal.begin() + static_cast<std::ptrdiff_t>(end), window.begin());
        return power_spectrum_db(window, K_DIRECT_FFT);
    }

    /// @brief Returns 10 log10 of the mean power per hertz of a K_DIRECT_FFT spectrum over the
    /// third octave centred on one frequency.
    double third_octave_db(const std::vector<double>& power_db, double centre_hz)
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

    /// @brief Returns the energy of a signal over [begin, end).
    double energy(const std::vector<float>& signal, std::size_t begin, std::size_t end)
    {
        double sum = 0.0;
        for (std::size_t frame = begin; frame < end; ++frame)
        {
            sum += static_cast<double>(signal[frame]) * signal[frame];
        }
        return sum;
    }

    /// @brief Returns the direct-to-reverberant ratio of an impulse response in decibels: the
    /// energy from two frames before the direct onset to 60 frames after it, against all the
    /// rest, which includes the standing waves that ring from frame 0.
    double direct_to_reverberant_db(const std::vector<float>& response, double onset_frames)
    {
        const std::size_t onset = static_cast<std::size_t>(std::floor(onset_frames));
        const double direct = energy(response, onset - 2, onset + 60);
        const double total = energy(response, 0, response.size());
        return 10.0 * std::log10(direct / (total - direct));
    }

    /// @brief Returns the 12 m dead room with the cabinet at its centre facing +x, so the
    /// nearest reflection, off the floor or the ceiling, arrives at least 12 m after the direct
    /// path and a window of 1500 frames holds the direct path alone for either listener: the
    /// ears' floor image lands at 1703 - 280 + 128 = 1551 with the propagation compensated.
    SushiDSP::room::RoomScene centred_dead_room()
    {
        SushiDSP::room::RoomScene scene = dead_room_scene();
        scene.source_position = {6.0, 6.0, 6.0};
        scene.listener_position = {8.0, 6.0, 6.0};
        scene.microphone_position = {9.0, 6.0, 6.0};
        scene.microphone_yaw_rad = K_PI;
        scene.microphone_pattern = SushiDSP::room::PolarPattern::Cardioid;
        return scene;
    }

    /// @brief Returns a cabinet small enough to act as a point: one 1 cm cone that never breaks
    /// up, so its seven elements stand within a centimetre and the far-field pattern is flat.
    SushiDSP::room::CabinetGeometry point_cabinet()
    {
        return SushiDSP::room::CabinetGeometry{0.01, 0.0, 0.05, 20000.0, 1, 1, 0.05, 0.05};
    }

    /// @brief Counts the frames of the direct-path window in centred_dead_room().
    constexpr std::size_t K_DIRECT_WINDOW = 1500;

    /// @brief Returns the direct-path power spectrum of a scene for one listener.
    std::vector<double> direct_power_db(const SushiDSP::room::RoomScene& scene, float listener)
    {
        std::unique_ptr<SushiDSP::room::RoomNode> node = prepared_node(scene, listener);
        std::vector<float> impulse(8 * K_BLOCK, 0.0f);
        impulse[0] = 1.0f;
        return windowed_power_db(render(*node, impulse), 0, K_DIRECT_WINDOW);
    }

    /// @brief Returns the first frame of [begin, end) whose magnitude exceeds a fraction of
    /// the window's peak, or -1 when the window is silent.
    long onset(const std::vector<float>& signal, std::size_t begin, std::size_t end,
               double fraction)
    {
        double peak = 0.0;
        for (std::size_t frame = begin; frame < end; ++frame)
        {
            peak = std::max(peak, static_cast<double>(std::fabs(signal[frame])));
        }
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

    /// @brief Returns what the node's direct path must render for an impulse into one ear: tap
    /// 0 of the ears set, compensated as the node publishes it, read past the alignment delay
    /// and the far ear's ITD through the renderer's kernel, its two sections, its gain, then
    /// the ear's minimum-phase HRIR.
    std::vector<float> reference_direct(const SushiDSP::room::RoomScene& scene, std::uint32_t ear,
                                        std::size_t frames)
    {
        const SushiDSP::room::HrtfSet hrtf =
            SushiDSP::room::SphericalHeadHrtf::build(K_SAMPLE_RATE);
        std::unique_ptr<SushiDSP::room::RoomRenderSet> set = SushiDSP::room::build_render_set(
            scene, hrtf, K_SAMPLE_RATE, SushiDSP::room::ListenerKind::Ears);
        SushiDSP::room::RoomNode::compensate_direct_delay(*set);
        const SushiDSP::room::EarlyTap& tap = set->early.taps[0];
        const double itd = set->direct_itd_frames;
        const double extra = ear == 0 ? std::max(-itd, 0.0) : std::max(itd, 0.0);
        std::vector<float> signal = lagrange_impulse(
            static_cast<double>(K_ALIGN_FRAMES) + tap.delay_frames + extra, tap.gain, frames);
        run_section(tap.section_a, signal);
        run_section(tap.section_b, signal);
        const std::array<float, SushiDSP::room::HrtfSet::K_TAP_COUNT>& hrir =
            ear == 0 ? set->direct_left : set->direct_right;
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

    /// @brief Returns the path of the converted SADIE II set, two directories above the
    /// fixtures; the cases that need it skip when it is absent.
    std::string measured_set_path()
    {
        return std::string(SUSHIDSP_ASSETS_DIR) + "/hrtf/sadie2_ku100_48k.hrtf";
    }

    /// @brief Returns the path of the six-direction fixture convert_hrtf.py's self-test writes.
    std::string fixture_set_path()
    {
        return std::string(SUSHIDSP_FIXTURE_DIR) + "/hrtf_self_test.hrtf";
    }

    /// @brief Holds the third-octave centres the decoder acceptance reads, 200 Hz to 8 kHz.
    constexpr std::array<double, 17> K_DECODER_BANDS = {
        200.0,  250.0,  315.0,  400.0,  500.0,  630.0,  800.0,  1000.0, 1250.0,
        1600.0, 2000.0, 2500.0, 3150.0, 4000.0, 5000.0, 6300.0, 8000.0};

    /// @brief Decodes one plane wave from a direction: an impulse encoded to sixteen channels,
    /// run through the decoder over frames frames, both ears returned.
    std::array<std::vector<float>, 2> decode_plane_wave(SushiDSP::room::BinauralDecoder& decoder,
                                                        double azimuth_rad, double elevation_rad,
                                                        std::size_t frames)
    {
        std::array<float, SushiDSP::room::AmbisonicEncoder::K_CHANNELS> gains{};
        SushiDSP::room::AmbisonicEncoder::encode(azimuth_rad, elevation_rad, gains);
        std::array<std::vector<float>, SushiDSP::room::AmbisonicEncoder::K_CHANNELS> channels;
        std::array<const float*, SushiDSP::room::AmbisonicEncoder::K_CHANNELS> pointers{};
        for (std::size_t channel = 0; channel < channels.size(); ++channel)
        {
            channels[channel].assign(K_BLOCK, 0.0f);
            pointers[channel] = channels[channel].data();
        }
        std::array<std::vector<float>, 2> ears = {std::vector<float>(frames, 0.0f),
                                                  std::vector<float>(frames, 0.0f)};
        std::vector<float> left(K_BLOCK);
        std::vector<float> right(K_BLOCK);
        decoder.reset();
        for (std::size_t offset = 0; offset + K_BLOCK <= frames; offset += K_BLOCK)
        {
            for (std::size_t channel = 0; channel < channels.size(); ++channel)
            {
                channels[channel][0] = offset == 0 ? gains[channel] : 0.0f;
            }
            decoder.process(pointers.data(), left.data(), right.data(), K_BLOCK);
            std::copy(left.begin(), left.end(),
                      ears[0].begin() + static_cast<std::ptrdiff_t>(offset));
            std::copy(right.begin(), right.end(),
                      ears[1].begin() + static_cast<std::ptrdiff_t>(offset));
        }
        return ears;
    }
} // namespace

TEST(Medium_RoomNode, RoomNodeDeclaresThreeInputSlotsAndOnePortPerReceiver)
{
    SushiDSP::room::RoomNode node;
    EXPECT_EQ(node.ports().input_channels, 6u);
    EXPECT_EQ(node.ports().output_channels, 2u);
    EXPECT_EQ(node.output_port_count(), 9u);
    EXPECT_EQ(node.output_port_channels(0), 2u);
    EXPECT_EQ(node.output_port_channels(1), 1u);
    EXPECT_EQ(node.output_port_channels(8), 1u);
    EXPECT_EQ(node.output_port_channels(9), 0u);
    EXPECT_STREQ(node.type_id(), "sushidsp.room");
}

TEST(Medium_RoomNode, RoomNodePlacesTheFirstOrderPeaksWhereTheSolverSaid)
{
    // Nothing but taps reaches the first 1749 frames. See tests/nodes/room/test_room_node/README.md.
    SushiDSP::room::RoomScene scene = microphone_scene();
    scene.cabinet = point_cabinet();
    std::unique_ptr<SushiDSP::room::RoomNode> node = prepared_node(scene, K_LISTENER_MICROPHONE);

    std::vector<float> impulse(2048, 0.0f);
    impulse[0] = 1.0f;
    const std::vector<float> response = render(*node, impulse);

    const std::array<double, 6> metres = {
        std::sqrt(2.5 * 2.5 + 0.7 * 0.7), std::sqrt(2.5 * 2.5 + 1.7 * 1.7),
        std::sqrt(2.5 * 2.5 + 3.3 * 3.3), std::sqrt(2.5 * 2.5 + 4.0 * 4.0 + 0.7 * 0.7),
        std::sqrt(5.5 * 5.5 + 0.7 * 0.7), std::sqrt(6.5 * 6.5 + 0.7 * 0.7)};
    const std::array<double, 6> plan_frames = {363.3098, 423.0778, 579.3654,
                                               667.3326, 775.8880, 914.8805};
    for (std::size_t path = 0; path < metres.size(); ++path)
    {
        const double frames = metres[path] * K_FRAMES_PER_METRE;
        EXPECT_NEAR(frames, plan_frames[path], 1e-3) << "path " << path;
        const long peak = nearest_peak(response, frames, 8, 1e-3f);
        ASSERT_GE(peak, 0) << "no peak near " << frames;
        EXPECT_NEAR(static_cast<double>(peak), frames, 0.5) << "path " << path;
    }
}

TEST(Medium_RoomNode, RoomNodeCrossfadesAPublishWithoutAStep)
{
    // The arriving bank starts from zero after a 0.36 m move. See tests/nodes/room/test_room_node/README.md.
    SushiDSP::room::RoomScene moved = microphone_scene();
    moved.microphone_position = {3.7, 2.2, 1.2};
    const CrossfadeMeasure measure = measure_crossfade(microphone_scene(), moved);
    RecordProperty("residual_dbfs", to_decibels(measure.residual));
    EXPECT_LE(measure.step_ratio, 1.01);
    EXPECT_LT(to_decibels(measure.residual), -44.0);
}

TEST(Medium_RoomNode, RoomNodeCrossfadesASmallMoveFromTheCarriedState)
{
    // 78 of the 96 taps carry their state over one centimetre. See tests/nodes/room/test_room_node/README.md.
    SushiDSP::room::RoomScene nudged = microphone_scene();
    nudged.microphone_position.x += 0.01;
    const CrossfadeMeasure measure = measure_crossfade(microphone_scene(), nudged);
    RecordProperty("residual_dbfs", to_decibels(measure.residual));
    EXPECT_LE(measure.step_ratio, 1.01);
    EXPECT_LT(to_decibels(measure.residual), -58.0);
}

TEST(Medium_RoomNode, RoomNodeRepublishesTheSameSceneWithoutAResidual)
{
    // The same scene refits to the same taps: zero residual. See tests/nodes/room/test_room_node/README.md.
    const CrossfadeMeasure measure = measure_crossfade(microphone_scene(), microphone_scene());
    RecordProperty("residual_dbfs", to_decibels(measure.residual));
    EXPECT_LE(measure.step_ratio, 1.0);
    EXPECT_LT(to_decibels(measure.residual), -120.0);
}

TEST(Medium_RoomNode, RoomNodeKeepsRenderingTheOldSetWhileASetIsBuilt)
{
    std::unique_ptr<SushiDSP::room::RoomNode> node =
        prepared_node(microphone_scene(), K_LISTENER_MICROPHONE);
    std::vector<float> left(K_BLOCK, 0.0f);
    std::vector<float> right(K_BLOCK, 0.0f);
    float* outputs[2] = {left.data(), right.data()};
    std::vector<float> block(K_BLOCK);

    std::size_t frame_count = 0;
    double quietest_rms = 1.0;
    for (int publish = 0; publish < 100; ++publish)
    {
        SushiDSP::room::RoomScene scene = microphone_scene();
        scene.microphone_position.x = 3.0 + 0.01 * publish;
        std::string reason;
        ASSERT_TRUE(publish_scene(*node, scene, reason)) << reason;

        for (int repeat = 0; repeat < 4; ++repeat)
        {
            for (std::uint32_t frame = 0; frame < K_BLOCK; ++frame, ++frame_count)
            {
                block[frame] = static_cast<float>(std::sin(
                    2.0 * K_PI * 1000.0 * static_cast<double>(frame_count) / K_SAMPLE_RATE));
            }
            const float* inputs[1] = {block.data()};
            SushiDSP::ProcessContext context{outputs, inputs, K_BLOCK, 0.0};
            node->process(context);

            double energy = 0.0;
            for (float sample : left)
            {
                ASSERT_TRUE(std::isfinite(sample));
                energy += static_cast<double>(sample) * sample;
            }
            quietest_rms = std::min(quietest_rms, std::sqrt(energy / K_BLOCK));
        }
        node->reclaim_rooms();
    }
    EXPECT_GT(to_decibels(quietest_rms), -80.0);
}

TEST(Medium_RoomNode, RoomNodeReclaimsARetiredSetOnlyAfterTheAudioThreadMovedOn)
{
    // The node never reads a slot pointer past its block. See tests/nodes/room/test_room_node/README.md.
    std::unique_ptr<SushiDSP::room::RoomNode> node =
        prepared_node(microphone_scene(), K_LISTENER_MICROPHONE);
    const std::vector<float> input = tone(4 * K_BLOCK);

    SushiDSP::room::RoomScene second = microphone_scene();
    second.microphone_position.x = 3.5;
    SushiDSP::room::RoomScene third = microphone_scene();
    third.microphone_position.x = 3.0;

    // The direct tap arrives 363 frames in. See tests/nodes/room/test_room_node/README.md.
    const std::vector<float> first_blocks =
        render(*node, std::vector<float>(input.begin(), input.begin() + 2 * K_BLOCK));
    const std::vector<float> first_block(first_blocks.begin() + K_BLOCK, first_blocks.end());
    std::string reason;
    ASSERT_TRUE(publish_scene(*node, second, reason)) << reason;
    ASSERT_TRUE(publish_scene(*node, third, reason)) << reason;
    node->reclaim_rooms();

    const std::vector<float> after_reclaim =
        render(*node, std::vector<float>(input.begin() + 2 * K_BLOCK, input.begin() + 3 * K_BLOCK));
    node->reclaim_rooms();
    const std::vector<float> settled =
        render(*node, std::vector<float>(input.begin() + 3 * K_BLOCK, input.begin() + 4 * K_BLOCK));

    for (const std::vector<float>* block : {&first_block, &after_reclaim, &settled})
    {
        double energy = 0.0;
        for (float sample : *block)
        {
            ASSERT_TRUE(std::isfinite(sample));
            energy += static_cast<double>(sample) * sample;
        }
        EXPECT_GT(energy, 0.0);
    }
    EXPECT_EQ(node->scene().microphone_position.x, 3.0);
}

TEST(Medium_RoomNode, RoomNodeRefusesAnInvalidSceneAndKeepsTheOldOne)
{
    std::unique_ptr<SushiDSP::room::RoomNode> node =
        prepared_node(microphone_scene(), K_LISTENER_MICROPHONE);

    SushiDSP::room::RoomScene invalid = microphone_scene();
    invalid.dimensions.x = 15.0;
    std::string reason;
    EXPECT_FALSE(publish_scene(*node, invalid, reason));
    EXPECT_FALSE(reason.empty());
    EXPECT_EQ(node->scene().dimensions.x, 6.0);
    EXPECT_EQ(node->scene().microphone_position.x, 4.0);

    // Before prepare() the scene is still validated and refused the same way.
    SushiDSP::room::RoomNode fresh;
    EXPECT_FALSE(publish_scene(fresh, invalid, reason));
    EXPECT_EQ(fresh.scene().dimensions.x, 6.0);
}

TEST(Medium_RoomNode, RoomNodeTakesTheDirectImageOutOfTheEarsBankAndLeavesTheMicrophones)
{
    // Tap 0 of a solved set is the direct image. See tests/nodes/room/test_room_node/README.md.
    const SushiDSP::room::HrtfSet hrtf = SushiDSP::room::SphericalHeadHrtf::build(K_SAMPLE_RATE);
    const SushiDSP::room::RoomScene scene = microphone_scene();

    std::unique_ptr<SushiDSP::room::RoomRenderSet> ears = SushiDSP::room::build_render_set(
        scene, hrtf, K_SAMPLE_RATE, SushiDSP::room::ListenerKind::Ears);
    const std::uint32_t solved = ears->early.count;
    const float direct = ears->early.taps[0].delay_frames;
    const float first_reflection = ears->early.taps[1].delay_frames;
    EXPECT_NEAR(direct, 363.3098f, 1e-2f);
    SushiDSP::room::EarlyTapSet reflections{};
    SushiDSP::room::EarlyTap direct_tap{};
    EXPECT_TRUE(SushiDSP::room::RoomNode::take_direct_image(*ears, reflections, direct_tap));
    EXPECT_EQ(reflections.count, solved - 1u);
    EXPECT_FLOAT_EQ(direct_tap.delay_frames, direct);
    EXPECT_FLOAT_EQ(reflections.taps[0].delay_frames, first_reflection);
    EXPECT_FLOAT_EQ(reflections.crossover_time_s, ears->early.crossover_time_s);
    for (std::uint32_t tap = 0; tap < reflections.count; ++tap)
    {
        EXPECT_GT(reflections.taps[tap].delay_frames, direct) << "tap " << tap;
    }

    std::unique_ptr<SushiDSP::room::RoomRenderSet> microphone = SushiDSP::room::build_render_set(
        scene, hrtf, K_SAMPLE_RATE, SushiDSP::room::ListenerKind::Microphone);
    direct_tap.delay_frames = -1.0f;
    EXPECT_FALSE(SushiDSP::room::RoomNode::take_direct_image(*microphone, reflections, direct_tap));
    EXPECT_EQ(reflections.count, microphone->early.count);
    EXPECT_FLOAT_EQ(direct_tap.delay_frames, -1.0f);
    // The bank opens with the cabinet's elements, the nearest nearer than the baffle's centre.
    EXPECT_EQ(reflections.direct_count, microphone->early.direct_count);
    EXPECT_EQ(microphone->early.direct_count, 28u);
    EXPECT_LT(reflections.taps[0].delay_frames, 363.3098f);
    EXPECT_GT(reflections.taps[0].delay_frames, 350.0f);
}

TEST(Medium_RoomNode, RoomNodeCompensatesAnEarsSetByItsRoundedPropagationDelay)
{
    // The ears' render moves earlier by round(d / c * fs). See tests/nodes/room/test_room_node/README.md.
    const SushiDSP::room::HrtfSet hrtf = SushiDSP::room::SphericalHeadHrtf::build(K_SAMPLE_RATE);
    const SushiDSP::room::RoomScene scene = microphone_scene();
    std::unique_ptr<SushiDSP::room::RoomRenderSet> ears = SushiDSP::room::build_render_set(
        scene, hrtf, K_SAMPLE_RATE, SushiDSP::room::ListenerKind::Ears);
    const SushiDSP::room::EarlyTapSet before = ears->early;
    const float tail_before = ears->late.delay_to_crossover_frames;
    EXPECT_FLOAT_EQ(ears->direct_delay_frames, 363.0f);
    SushiDSP::room::RoomNode::compensate_direct_delay(*ears);
    EXPECT_NEAR(ears->early.taps[0].delay_frames, 363.3098f - 363.0f, 1e-3f);
    for (std::uint32_t tap = 1; tap < ears->early.count; ++tap)
    {
        EXPECT_FLOAT_EQ(ears->early.taps[tap].delay_frames,
                        std::max(before.taps[tap].delay_frames - 363.0f, 1.0f))
            << "tap " << tap;
    }
    EXPECT_FLOAT_EQ(ears->late.delay_to_crossover_frames, std::max(tail_before - 363.0f, 0.0f));

    std::unique_ptr<SushiDSP::room::RoomRenderSet> microphone = SushiDSP::room::build_render_set(
        scene, hrtf, K_SAMPLE_RATE, SushiDSP::room::ListenerKind::Microphone);
    const float microphone_direct = microphone->early.taps[0].delay_frames;
    const float microphone_tail = microphone->late.delay_to_crossover_frames;
    SushiDSP::room::RoomNode::compensate_direct_delay(*microphone);
    EXPECT_FLOAT_EQ(microphone->early.taps[0].delay_frames, microphone_direct);
    EXPECT_FLOAT_EQ(microphone->late.delay_to_crossover_frames, microphone_tail);
}

TEST(Medium_RoomNode, RoomNodeRingsAtTheGaragesEigenfrequencies)
{
    // The first six modes lie between 28.583 and 71.458 Hz. See tests/nodes/room/test_room_node/README.md.
    const std::vector<double> power = power_spectrum_db(wave_response(wave_scene()), K_WAVE_FFT);
    const std::vector<SpectralPeak> peaks = find_peaks(power, 20.0, 78.0, 6.0);
    const std::array<double, 6> expected = {28.5833, 42.8750, 51.5293, 57.1667, 68.6000, 71.4583};
    ASSERT_EQ(peaks.size(), expected.size());
    for (std::size_t index = 0; index < expected.size(); ++index)
    {
        EXPECT_NEAR(peaks[index].frequency_hz, expected[index], 0.005 * expected[index])
            << "peak " << index;
    }
}

TEST(Medium_RoomNode, RoomNodeModeWidthsMatchTheirDamping)
{
    // A resonator's -3 dB width is 2.199 / T60 Hz. See tests/nodes/room/test_room_node/README.md.
    const SushiDSP::room::RoomScene scene = wave_scene();
    const std::vector<SushiDSP::room::ModalSolver::Mode> modes =
        SushiDSP::room::ModalSolver::enumerate(scene, 70.0);
    const std::vector<double> power = power_spectrum_db(wave_response(scene), K_WAVE_FFT);
    const std::vector<SpectralPeak> peaks = find_peaks(power, 20.0, 70.0, 6.0);
    ASSERT_EQ(peaks.size(), 5u);
    ASSERT_EQ(modes.size(), 5u);
    for (std::size_t index = 0; index < peaks.size(); ++index)
    {
        const double expected = 3.0 * std::log(10.0) / (K_PI * modes[index].decay_seconds);
        RecordProperty("width_" + std::to_string(index), peaks[index].width_hz);
        EXPECT_NEAR(peaks[index].width_hz, expected, 0.2 * expected) << "mode " << index;
    }
}

TEST(Medium_RoomNode, RoomNodePressesTheBassIntoTheCorner)
{
    // Every mode with an odd index has a node at the centre. See tests/nodes/room/test_room_node/README.md.
    SushiDSP::room::RoomScene corner = wave_scene();
    corner.listener_position = {0.15, 0.15, 0.15};
    SushiDSP::room::RoomScene centre = wave_scene();
    centre.listener_position = {3.0, 2.0, 1.25};
    const double corner_db =
        band_density_db(power_spectrum_db(wave_response(corner), K_WAVE_FFT), 20.0, 100.0);
    const double centre_db =
        band_density_db(power_spectrum_db(wave_response(centre), K_WAVE_FFT), 20.0, 100.0);
    RecordProperty("corner_minus_centre_db", corner_db - centre_db);
    EXPECT_GE(corner_db - centre_db, 12.0);
}

namespace
{
    /// @brief Holds the frames one seam render runs for: 1.37 s, which a 1.9 s decay leaves
    /// 10^-4.3 of its energy past, with bins of 0.73 Hz against mode widths of over 1 Hz.
    constexpr std::uint32_t K_SEAM_FRAMES = 65536;

    /// @brief Holds the poses one room's seam is read at, as fractions of its three lengths:
    /// the cabinet's then the listener's, none on a symmetry plane, cabinets near the floor.
    constexpr std::array<std::array<double, 6>, 6> K_SEAM_POSES = {{
        {0.07, 0.12, 0.20, 0.90, 0.85, 0.80},
        {0.30, 0.20, 0.25, 0.65, 0.70, 0.55},
        {0.15, 0.80, 0.30, 0.55, 0.30, 0.65},
        {0.45, 0.35, 0.18, 0.80, 0.60, 0.45},
        {0.85, 0.15, 0.22, 0.25, 0.75, 0.60},
        {0.60, 0.88, 0.28, 0.20, 0.35, 0.70},
    }};

    /// @brief Holds one octave's rendered power and the power the design says it holds.
    struct OctavePower
    {
            /// @brief Stores the rendered mean power per hertz.
            double rendered;

            /// @brief Stores the mean power per hertz the design predicts.
            double expected;
    };

    /// @brief Returns the fourth-order Butterworth low half's power at f for a crossover at
    /// fc, 1 / (1 + (f / fc)^8); the high half's is (f / fc)^8 times it, so the two sum to one.
    /// The pair splits a modal field from a geometric one, which are uncorrelated, so it keeps
    /// the power identity rather than the amplitude one a Linkwitz-Riley pair keeps.
    double butterworth_low_power(double f, double fc)
    {
        return 1.0 / (1.0 + std::pow(f / fc, 8.0));
    }

    /// @brief Returns the mean of a K_SEAM_FRAMES power spectrum over [lower, upper) hertz.
    double seam_band_power(const std::vector<double>& power_db, double lower, double upper)
    {
        const double bin = K_SAMPLE_RATE / K_SEAM_FRAMES;
        const auto first = static_cast<std::size_t>(std::ceil(lower / bin));
        const auto last = static_cast<std::size_t>(std::ceil(upper / bin));
        double sum = 0.0;
        for (std::size_t index = first; index < last; ++index)
        {
            sum += std::pow(10.0, power_db[index] / 10.0);
        }
        return sum / static_cast<double>(last - first);
    }

    /// @brief Returns the modal octave's power per hertz the design holds at its pose: each
    /// mode under f_s / 2 left out, the rest summed as the section's white-noise gain
    /// 2 / (1 - r^2) times b0^2 and psi^2 at the cabinet and the receiver, through the low
    /// half at the mode's frequency, times fs / 2 over the octave's width.
    double modal_pose_power(const SushiDSP::room::RoomRenderSet& set,
                            const SushiDSP::room::RoomScene& scene)
    {
        const double crossover = set.crossover_hz;
        const std::vector<SushiDSP::room::ModalSolver::Mode> modes =
            SushiDSP::room::ModalSolver::enumerate(scene, crossover);
        double sum = 0.0;
        for (std::size_t index = 0; index < modes.size() && index < set.modal.count; ++index)
        {
            if (modes[index].frequency_hz < 0.5 * crossover)
            {
                continue;
            }
            const SushiDSP::room::ModalDesign::Mode& mode = set.modal.modes[index];
            const double psi = static_cast<double>(mode.source_gain) * mode.source_gain *
                               static_cast<double>(mode.ear_gain[0]) * mode.ear_gain[0];
            sum += mode.section[0] * mode.section[0] * 2.0 / (1.0 - mode.section[4]) * psi *
                   butterworth_low_power(modes[index].frequency_hz, crossover);
        }
        return sum * (0.5 * K_SAMPLE_RATE) / (0.5 * crossover);
    }

    /// @brief Returns the geometric octave's power per hertz the reference holds: the diffuse
    /// field through the high half, averaged over the octave above f_s on the render's bins.
    double geometric_reference_power(const SushiDSP::room::DiffuseField& field, double crossover)
    {
        const double bin = K_SAMPLE_RATE / K_SEAM_FRAMES;
        const auto first = static_cast<std::size_t>(std::ceil(crossover / bin));
        const auto last = static_cast<std::size_t>(std::ceil(2.0 * crossover / bin));
        double sum = 0.0;
        for (std::size_t index = first; index < last; ++index)
        {
            const double f = static_cast<double>(index) * bin;
            sum += field.power_at(f) * std::pow(f / crossover, 8.0) *
                   butterworth_low_power(f, crossover);
        }
        return sum / static_cast<double>(last - first);
    }

    /// @brief Renders one room's microphone response at every seam pose and returns the two
    /// octaves' rendered and expected powers, each averaged over the poses as energy.
    std::array<OctavePower, 2> seam_octaves(const SushiDSP::room::RoomScene& room)
    {
        const SushiDSP::room::HrtfSet hrtf =
            SushiDSP::room::SphericalHeadHrtf::build(K_SAMPLE_RATE);
        const SushiDSP::room::DiffuseField field(room);
        const SushiDSP::room::Vec3 length = room.dimensions;
        std::array<OctavePower, 2> octaves{};
        for (const std::array<double, 6>& pose : K_SEAM_POSES)
        {
            SushiDSP::room::RoomScene scene = room;
            scene.source_position = {pose[0] * length.x, pose[1] * length.y, pose[2] * length.z};
            scene.listener_position = {pose[3] * length.x, pose[4] * length.y, pose[5] * length.z};
            scene.microphone_position = scene.listener_position;
            scene.microphone_pattern = SushiDSP::room::PolarPattern::Omni;
            const std::unique_ptr<SushiDSP::room::RoomRenderSet> set =
                SushiDSP::room::build_render_set(scene, hrtf, K_SAMPLE_RATE,
                                                 SushiDSP::room::ListenerKind::Microphone);
            const double crossover = set->crossover_hz;
            std::unique_ptr<SushiDSP::room::RoomNode> node =
                prepared_node(scene, K_LISTENER_MICROPHONE);
            std::vector<float> impulse(K_SEAM_FRAMES, 0.0f);
            impulse[0] = 1.0f;
            const std::vector<double> power =
                power_spectrum_db(render(*node, impulse), K_SEAM_FRAMES);
            const double poses = static_cast<double>(K_SEAM_POSES.size());
            octaves[0].rendered += seam_band_power(power, 0.5 * crossover, crossover) / poses;
            octaves[0].expected += modal_pose_power(*set, scene) / poses;
            octaves[1].rendered += seam_band_power(power, crossover, 2.0 * crossover) / poses;
            octaves[1].expected += geometric_reference_power(field, crossover) / poses;
        }
        return octaves;
    }
} // namespace

TEST(Medium_RoomNode, RoomNodeIsContinuousAcrossTheSchroederFrequency)
{
    // Both layers carry DiffuseField's level at f_s. See tests/nodes/room/test_room_node/README.md.
    const std::array<SushiDSP::room::RoomScene, 3> rooms = {SushiDSP::test::garage_scene(),
                                                            SushiDSP::test::bedroom_scene(),
                                                            SushiDSP::test::block_room_scene()};
    const std::array<const char*, 3> names = {"garage", "bedroom", "block"};
    for (std::size_t room = 0; room < rooms.size(); ++room)
    {
        const std::array<OctavePower, 2> octaves = seam_octaves(rooms[room]);
        const double modal_db = 10.0 * std::log10(octaves[0].rendered / octaves[0].expected);
        const double geometric_db = 10.0 * std::log10(octaves[1].rendered / octaves[1].expected);
        const double step_db = 10.0 * std::log10(octaves[0].rendered / octaves[1].rendered);
        const std::string name = names[room];
        RecordProperty(name + "_modal_against_design_db", modal_db);
        RecordProperty(name + "_geometric_against_reference_db", geometric_db);
        RecordProperty(name + "_octave_step_db", step_db);
        EXPECT_NEAR(modal_db, 0.0, 2.0) << name;
        EXPECT_NEAR(geometric_db, 0.0, 2.0) << name;
    }
}

TEST(Medium_RoomNode, RoomNodeSwitchesTheWaveLayerOffInADeadRoom)
{
    // f_s under 40 Hz switches the modal layer off. See tests/nodes/room/test_room_node/README.md.
    const SushiDSP::room::RoomScene scene = dead_room_scene();
    const double schroeder = SushiDSP::room::EyringDecay::schroeder_frequency_hz(scene);
    ASSERT_LT(schroeder, 40.0);
    const SushiDSP::room::HrtfSet hrtf = SushiDSP::room::SphericalHeadHrtf::build(K_SAMPLE_RATE);
    std::unique_ptr<SushiDSP::room::RoomRenderSet> set = SushiDSP::room::build_render_set(
        scene, hrtf, K_SAMPLE_RATE, SushiDSP::room::ListenerKind::Microphone);
    ASSERT_EQ(set->modal.count, 0u);
    ASSERT_EQ(set->crossover_hz, 0.0f);

    constexpr std::uint32_t K_BLOCKS = 40;
    SushiDSP::room::EarlyReflectionRenderer early;
    early.prepare(8397, K_BLOCK);
    SushiDSP::room::DelayRing early_ring;
    early_ring.prepare(8397, K_BLOCK);
    SushiDSP::room::LateReverb late;
    late.prepare(25200);
    std::array<std::vector<float>, SushiDSP::room::EarlyReflectionRenderer::K_OUTPUT_CHANNELS>
        channels;
    std::array<float*, SushiDSP::room::EarlyReflectionRenderer::K_OUTPUT_CHANNELS> pointers{};
    for (std::size_t channel = 0; channel < channels.size(); ++channel)
    {
        channels[channel].assign(K_BLOCK, 0.0f);
        pointers[channel] = channels[channel].data();
    }
    std::vector<float> tail(K_BLOCK);

    std::unique_ptr<SushiDSP::room::RoomNode> node = prepared_node(scene, K_LISTENER_MICROPHONE);
    EXPECT_DOUBLE_EQ(node->schroeder_frequency_hz(), schroeder);
    const std::vector<float> input = tone(K_BLOCKS * K_BLOCK);
    const std::vector<float> heard = render(*node, input);
    std::vector<float> expected(input.size(), 0.0f);
    for (std::uint32_t block = 0; block < K_BLOCKS; ++block)
    {
        const std::size_t offset = static_cast<std::size_t>(block) * K_BLOCK;
        const float* block_input = input.data() + offset;
        for (std::vector<float>& channel : channels)
        {
            std::fill(channel.begin(), channel.end(), 0.0f);
        }
        early_ring.write(block_input, K_BLOCK);
        early.render(early_ring, set->early, 0, pointers.data(), K_BLOCK);
        late.process(set->late, block_input, tail.data(), tail.data(), K_BLOCK);
        for (std::uint32_t frame = 0; frame < K_BLOCK; ++frame)
        {
            expected[offset + frame] = channels[0][frame] + tail[frame];
        }
    }
    // The node applies the tail's level after the network, so the two differ by rounding alone.
    double worst = 0.0;
    double peak = 0.0;
    for (std::size_t frame = 0; frame + K_ALIGN_FRAMES < input.size(); ++frame)
    {
        worst = std::max(worst, static_cast<double>(std::fabs(heard[frame] - expected[frame])));
        peak = std::max(peak, static_cast<double>(std::fabs(expected[frame])));
    }
    ASSERT_GT(peak, 1e-3);
    EXPECT_LT(worst, 1e-5 * std::max(peak, 1.0));
}

TEST(Medium_RoomNode, RoomNodePublishesNewModalGainsWithoutRebuildingAResonator)
{
    // A moved cabinet or listener rebuilds no resonator. See tests/nodes/room/test_room_node/README.md.
    const SushiDSP::room::HrtfSet hrtf = SushiDSP::room::SphericalHeadHrtf::build(K_SAMPLE_RATE);
    const auto build = [&hrtf](const SushiDSP::room::RoomScene& scene)
    {
        return SushiDSP::room::build_render_set(scene, hrtf, K_SAMPLE_RATE,
                                                SushiDSP::room::ListenerKind::Ears);
    };
    const SushiDSP::room::RoomScene scene = wave_scene();
    SushiDSP::room::RoomScene cabinet_moved = scene;
    cabinet_moved.source_position.x += 0.2;
    SushiDSP::room::RoomScene listener_moved = scene;
    listener_moved.listener_position.y -= 0.3;
    SushiDSP::room::RoomScene widened = scene;
    widened.dimensions.x += 0.1;

    const std::unique_ptr<SushiDSP::room::RoomRenderSet> before = build(scene);
    const std::unique_ptr<SushiDSP::room::RoomRenderSet> cabinet = build(cabinet_moved);
    const std::unique_ptr<SushiDSP::room::RoomRenderSet> listener = build(listener_moved);
    const std::unique_ptr<SushiDSP::room::RoomRenderSet> wider = build(widened);
    ASSERT_EQ(before->modal.count, 187u);
    EXPECT_TRUE(SushiDSP::room::RoomNode::same_resonators(before->modal, cabinet->modal));
    EXPECT_TRUE(SushiDSP::room::RoomNode::same_resonators(before->modal, listener->modal));
    EXPECT_FALSE(SushiDSP::room::RoomNode::same_resonators(before->modal, wider->modal));
    EXPECT_EQ(before->modal.level_scale, cabinet->modal.level_scale);

    // 161 of the 187 modes have a non-zero x index. See tests/nodes/room/test_room_node/README.md.
    const std::vector<SushiDSP::room::ModalSolver::Mode> modes =
        SushiDSP::room::ModalSolver::enumerate(scene, before->crossover_hz);
    ASSERT_EQ(modes.size(), before->modal.count);
    std::uint32_t along_x = 0;
    std::uint32_t along_y = 0;
    for (const SushiDSP::room::ModalSolver::Mode& mode : modes)
    {
        along_x += mode.nx != 0u ? 1u : 0u;
        along_y += mode.ny != 0u ? 1u : 0u;
    }
    std::uint32_t source_moved = 0;
    std::uint32_t ear_moved = 0;
    for (std::uint32_t index = 0; index < before->modal.count; ++index)
    {
        const SushiDSP::room::ModalDesign::Mode& base = before->modal.modes[index];
        EXPECT_EQ(base.section, cabinet->modal.modes[index].section) << "mode " << index;
        EXPECT_EQ(base.ear_gain, cabinet->modal.modes[index].ear_gain) << "mode " << index;
        EXPECT_EQ(base.source_gain, listener->modal.modes[index].source_gain) << "mode " << index;
        source_moved += base.source_gain != cabinet->modal.modes[index].source_gain ? 1u : 0u;
        ear_moved += base.ear_gain != listener->modal.modes[index].ear_gain ? 1u : 0u;
    }
    EXPECT_EQ(along_x, 161u);
    EXPECT_EQ(along_y, 150u);
    EXPECT_EQ(source_moved, along_x);
    EXPECT_EQ(ear_moved, along_y);
}

namespace
{
    /// @brief Holds what one publish under a standing wave measures.
    struct WaveMeasure
    {
            /// @brief Stores the moving render's largest sample-to-sample step over the fade,
            /// divided by the reference's own.
            double step_ratio;

            /// @brief Stores the largest difference between the moving render and the reference
            /// from the swap to one block past the fade, relative to the reference's peak.
            double residual_db;

            /// @brief Stores the moving render's energy over the four blocks after the fade
            /// against the arriving scene's steady render, in decibels.
            double settled_db;
    };

    /// @brief Drives three nodes heard by the ears with a tone at 68.6 Hz, the (0, 0, 1)
    /// mode: one in scene a throughout, one in scene b throughout, and one that moves from a
    /// to b at block 200; the reference is the raised-cosine blend of the two steady renders
    /// over the fade.
    WaveMeasure measure_wave(const SushiDSP::room::RoomScene& scene_a,
                             const SushiDSP::room::RoomScene& scene_b)
    {
        constexpr std::uint32_t K_SWAP_BLOCK = 200;
        constexpr std::size_t K_FRAMES = 240 * K_BLOCK;
        std::vector<float> input(K_FRAMES);
        for (std::size_t frame = 0; frame < K_FRAMES; ++frame)
        {
            input[frame] = static_cast<float>(
                std::sin(2.0 * K_PI * 68.6 * static_cast<double>(frame) / K_SAMPLE_RATE));
        }
        const std::vector<float> steady_a = render(*prepared_node(scene_a, K_LISTENER_EARS), input);
        const std::vector<float> steady_b = render(*prepared_node(scene_b, K_LISTENER_EARS), input);
        const std::vector<float> moving = render(*prepared_node(scene_a, K_LISTENER_EARS), input,
                                                 {K_SWAP_BLOCK}, scene_edit(scene_b));

        // The ears' fade starts one partition after the edit, with the decoder's output.
        const std::size_t swap = static_cast<std::size_t>(K_SWAP_BLOCK) * K_BLOCK + K_ALIGN_FRAMES;
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
        double moving_energy = 0.0;
        double steady_energy = 0.0;
        for (std::size_t frame = fade_end; frame < fade_end + 4 * K_BLOCK; ++frame)
        {
            moving_energy += static_cast<double>(moving[frame]) * moving[frame];
            steady_energy += static_cast<double>(steady_b[frame]) * steady_b[frame];
        }
        return {moving_step / own_step, to_decibels(residual / peak),
                10.0 * std::log10(moving_energy / steady_energy)};
    }
} // namespace

TEST(Medium_RoomNode, RoomNodeCarriesTheStandingWavesAcrossAMovedCabinet)
{
    // The bank keeps its state; only the source gains ramp. See tests/nodes/room/test_room_node/README.md.
    const SushiDSP::room::RoomScene scene = wave_scene();
    SushiDSP::room::RoomScene cabinet_moved = scene;
    cabinet_moved.source_position.x += 0.2;
    const WaveMeasure measure = measure_wave(scene, cabinet_moved);
    RecordProperty("residual_db", measure.residual_db);
    EXPECT_LE(measure.step_ratio, 1.01);
    EXPECT_NEAR(measure.settled_db, 0.0, 0.5);
}

TEST(Medium_RoomNode, RoomNodeRampsTheEarGainsAsTheOutputBlendWould)
{
    // The bank's output is linear in the ear gains. See tests/nodes/room/test_room_node/README.md.
    const SushiDSP::room::RoomScene scene = wave_scene();
    SushiDSP::room::RoomScene listener_moved = scene;
    listener_moved.listener_position.y -= 0.3;
    const WaveMeasure measure = measure_wave(scene, listener_moved);
    RecordProperty("residual_db", measure.residual_db);
    EXPECT_LE(measure.step_ratio, 1.0 + 1e-6);
    EXPECT_LT(measure.residual_db, -70.0);
}

TEST(Medium_RoomNode, RoomNodeRebuildsTheStandingWavesInAChangedRoom)
{
    // A changed room starts the spare bank from zero. See tests/nodes/room/test_room_node/README.md.
    const SushiDSP::room::RoomScene scene = wave_scene();
    SushiDSP::room::RoomScene widened = scene;
    widened.dimensions.x += 0.1;
    const WaveMeasure measure = measure_wave(scene, widened);
    RecordProperty("settled_db", measure.settled_db);
    EXPECT_LT(measure.settled_db, -6.0);
}

TEST(Medium_RoomNode, RoomNodeReportsTheCrossoverTheSetSplitsAt)
{
    // The node reports the crossover its seam sits at. See tests/nodes/room/test_room_node/README.md.
    const SushiDSP::room::RoomScene garage = wave_scene();
    const std::unique_ptr<SushiDSP::room::RoomNode> node =
        prepared_node(garage, K_LISTENER_MICROPHONE);
    EXPECT_NEAR(node->schroeder_frequency_hz(),
                SushiDSP::room::EyringDecay::schroeder_frequency_hz(garage), 1e-4);

    const SushiDSP::room::RoomScene small = small_concrete_room();
    const double eyring = SushiDSP::room::EyringDecay::schroeder_frequency_hz(small);
    ASSERT_GT(eyring, 800.0);
    const auto design =
        std::make_unique<SushiDSP::room::ModalDesign>(SushiDSP::room::ModalSolver::design(
            small, K_SAMPLE_RATE, eyring, SushiDSP::room::ListenerKind::Microphone));
    ASSERT_EQ(design->count, SushiDSP::room::ModalDesign::K_MAX_MODES);
    std::string reason;
    ASSERT_TRUE(publish_scene(*node, small, reason)) << reason;
    EXPECT_LT(node->schroeder_frequency_hz(), eyring);
    EXPECT_DOUBLE_EQ(node->schroeder_frequency_hz(), static_cast<double>(design->crossover_hz));

    const SushiDSP::room::RoomScene dead = dead_room_scene();
    ASSERT_TRUE(publish_scene(*node, dead, reason)) << reason;
    EXPECT_DOUBLE_EQ(node->schroeder_frequency_hz(),
                     SushiDSP::room::EyringDecay::schroeder_frequency_hz(dead));
    EXPECT_LT(node->schroeder_frequency_hz(), 40.0);
    EXPECT_GT(node->schroeder_frequency_hz(), 0.0);

    // Before prepare() there is no set; the report is Eyring's figure for the scene in place.
    SushiDSP::room::RoomNode fresh;
    ASSERT_TRUE(publish_scene(fresh, small, reason)) << reason;
    EXPECT_DOUBLE_EQ(fresh.schroeder_frequency_hz(), eyring);
}

TEST(Medium_RoomNode, RoomNodeMicrophoneListenerFillsBothChannelsIdentically)
{
    // The microphone's two outputs are equal bit for bit. See tests/nodes/room/test_room_node/README.md.
    SushiDSP::room::RoomScene scene = wave_scene();
    scene.listener_position = {2.0, 1.0, 1.0};
    scene.microphone_pattern = SushiDSP::room::PolarPattern::Cardioid;
    SushiDSP::room::RoomScene listener_moved = scene;
    listener_moved.listener_position = {4.0, 3.0, 1.5};

    std::unique_ptr<SushiDSP::room::RoomNode> node = prepared_node(scene, K_LISTENER_MICROPHONE);
    std::vector<float> input = tone(16 * K_BLOCK);
    input[0] = 1.0f;
    const std::array<std::vector<float>, 2> output =
        render_both(*node, input, {6}, scene_edit(listener_moved));
    ASSERT_EQ(output[0].size(), 4096u);
    double peak = 0.0;
    for (std::size_t frame = 0; frame < output[0].size(); ++frame)
    {
        ASSERT_EQ(output[0][frame], output[1][frame]) << "frame " << frame;
        peak = std::max(peak, static_cast<double>(std::fabs(output[0][frame])));
    }
    EXPECT_GT(peak, 0.0);
}

TEST(Medium_RoomNode, RoomNodeMicrophoneAtFiveCentimetresLiftsTheBottomEnd)
{
    // The tilt is expected at 13.68 dB against a point source. See tests/nodes/room/test_room_node/README.md.
    SushiDSP::room::RoomScene close_up = centred_dead_room();
    close_up.cabinet = point_cabinet();
    close_up.microphone_position = {6.05, 6.0, 6.0};
    SushiDSP::room::RoomScene distant = centred_dead_room();
    distant.cabinet = point_cabinet();
    const std::vector<double> close_db = direct_power_db(close_up, K_LISTENER_MICROPHONE);
    const std::vector<double> distant_db = direct_power_db(distant, K_LISTENER_MICROPHONE);

    const auto tilt = [](const std::vector<double>& power, double band_hz)
    {
        return third_octave_db(power, band_hz) - third_octave_db(power, 4000.0);
    };
    const auto cardioid_db = [](double frequency_hz, double distance_m)
    {
        return 20.0 * std::log10(0.5 + 0.5 * SushiDSP::room::MicrophoneModel::proximity_boost(
                                                 frequency_hz, distance_m));
    };
    const auto air_db = [](double distance_m)
    {
        const SushiDSP::room::BandResponse air =
            SushiDSP::room::AirAbsorption::over_path(distance_m);
        return 20.0 * std::log10(static_cast<double>(air.gain[0]) / air.gain[5]);
    };
    const auto model_tilt = [&](double band_hz)
    {
        return cardioid_db(band_hz, 0.05) - cardioid_db(4000.0, 0.05) -
               (cardioid_db(band_hz, 3.0) - cardioid_db(4000.0, 3.0)) + air_db(3.0) - air_db(0.05);
    };
    const double expected = model_tilt(125.0);
    const double measured = tilt(close_db, 125.0) - tilt(distant_db, 125.0);
    const double at_100_hz = tilt(close_db, 100.0) - tilt(distant_db, 100.0);
    RecordProperty("expected_db", expected);
    RecordProperty("measured_db", measured);
    RecordProperty("at_100_hz_db", at_100_hz);
    EXPECT_NEAR(expected, 13.68, 0.05);
    EXPECT_NEAR(model_tilt(100.0), 15.39, 0.05);
    EXPECT_NEAR(measured, expected, 1.0);
    EXPECT_GT(at_100_hz, measured);
}

TEST(Medium_RoomNode, RoomNodeMicrophoneHearsLessRoomThanTheEars)
{
    // The two ratios differ by at least 11.5 dB. See tests/nodes/room/test_room_node/README.md.
    SushiDSP::room::RoomScene scene = SushiDSP::test::garage_scene();
    scene.source_position = {1.5, 2.0, 0.5};
    scene.microphone_position = {1.55, 2.0, 0.5};
    scene.microphone_yaw_rad = K_PI;
    scene.microphone_pattern = SushiDSP::room::PolarPattern::Cardioid;
    scene.listener_position = {3.5, 2.0, 1.2};

    std::vector<float> impulse(188 * K_BLOCK, 0.0f);
    impulse[0] = 1.0f;
    const std::vector<float> microphone =
        render(*prepared_node(scene, K_LISTENER_MICROPHONE), impulse);
    const std::vector<float> ears = render(*prepared_node(scene, K_LISTENER_EARS), impulse);

    const double microphone_db = direct_to_reverberant_db(microphone, 0.05 * K_FRAMES_PER_METRE);
    const double ears_db =
        direct_to_reverberant_db(ears, compensated_direct_frames(std::sqrt(4.0 + 0.49)));
    RecordProperty("microphone_db", microphone_db);
    RecordProperty("ears_db", ears_db);
    // Between the four cones the difference measures 12.20 dB. See tests/nodes/room/test_room_node/README.md.
    EXPECT_GE(microphone_db - ears_db, 11.5);
}

TEST(Medium_RoomNode, RoomNodeCarriesOneOverROnTheCabinetsAxis)
{
    // An omni capsule on a point source's axis reads 1 / r. See tests/nodes/room/test_room_node/README.md.
    std::array<double, 3> measured{};
    const std::array<double, 3> distance = {1.0, 2.0, 4.0};
    for (std::size_t step = 0; step < distance.size(); ++step)
    {
        SushiDSP::room::RoomScene scene = centred_dead_room();
        scene.cabinet = point_cabinet();
        scene.microphone_position = {6.0 + distance[step], 6.0, 6.0};
        scene.microphone_pattern = SushiDSP::room::PolarPattern::Omni;
        std::vector<float> impulse(8 * K_BLOCK, 0.0f);
        impulse[0] = 1.0f;
        const std::vector<float> out =
            render(*prepared_node(scene, K_LISTENER_MICROPHONE), impulse);
        double energy = 0.0;
        for (std::size_t frame = 0; frame < K_DIRECT_WINDOW && frame < out.size(); ++frame)
        {
            energy += static_cast<double>(out[frame]) * out[frame];
        }
        measured[step] = 10.0 * std::log10(std::max(energy, 1e-30));
        RecordProperty("direct_db_" + std::to_string(step), measured[step]);
        EXPECT_NEAR(measured[step], 20.0 * std::log10(1.0 / distance[step]), 1.5)
            << "at " << distance[step] << " m";
    }
    EXPECT_NEAR(measured[0] - measured[1], 6.02, 1.0);
    EXPECT_NEAR(measured[1] - measured[2], 6.02, 1.0);
}

TEST(Medium_RoomNode, RoomNodeMovingAroundTheCabinetChangesTheTone)
{
    // The listener faces the cabinet at both spots. See tests/nodes/room/test_room_node/README.md.
    SushiDSP::room::RoomScene on_axis = centred_dead_room();
    on_axis.listener_yaw_rad = K_PI;
    SushiDSP::room::RoomScene off_axis = on_axis;
    off_axis.listener_position = {7.0, 6.0 + 2.0 * std::sin(K_PI / 3.0), 6.0};
    off_axis.listener_yaw_rad =
        std::atan2(6.0 - off_axis.listener_position.y, 6.0 - off_axis.listener_position.x);
    const std::vector<double> on_db = direct_power_db(on_axis, K_LISTENER_EARS);
    const std::vector<double> off_db = direct_power_db(off_axis, K_LISTENER_EARS);

    const double drop_2k = third_octave_db(on_db, 2000.0) - third_octave_db(off_db, 2000.0);
    const double move_125 = third_octave_db(off_db, 125.0) - third_octave_db(on_db, 125.0);
    const SushiDSP::room::CabinetDirectivity cabinet(SushiDSP::room::K_CABINET4X12);
    const SushiDSP::room::BandResponse model = cabinet.gain_for(K_PI / 3.0, 0.0);
    const double model_2k = -20.0 * std::log10(static_cast<double>(model.gain[4]));
    const double model_125 = 20.0 * std::log10(static_cast<double>(model.gain[0]));
    RecordProperty("drop_2k_db", drop_2k);
    RecordProperty("move_125_db", move_125);
    RecordProperty("model_2k_db", model_2k);
    RecordProperty("model_125_db", model_125);
    EXPECT_GT(drop_2k, 6.0);
    EXPECT_LT(std::fabs(move_125), 1.0);
    EXPECT_NEAR(drop_2k, model_2k, 1.0);
    EXPECT_NEAR(move_125, model_125, 1.0);
}

TEST(Medium_RoomNode, RoomNodeEarsHearTheDirectSoundThroughTheHrirPair)
{
    // The floor image follows at frame 1551. See tests/nodes/room/test_room_node/README.md.
    const SushiDSP::room::RoomScene scene = centred_dead_room();
    std::vector<float> impulse(4096, 0.0f);
    impulse[0] = 1.0f;
    const std::array<std::vector<float>, 2> ears =
        render_both(*prepared_node(scene, K_LISTENER_EARS), impulse);
    const std::vector<float> reference_left = reference_direct(scene, 0, 1500);
    const std::vector<float> reference_right = reference_direct(scene, 1, 1500);

    const double direct = compensated_direct_frames(2.0);
    const long peak = nearest_peak(ears[0], direct, 3, 1e-3f);
    ASSERT_GE(peak, 0);
    EXPECT_NEAR(static_cast<double>(peak), direct, 2.0);
    for (std::size_t frame = 0; frame + 3 < static_cast<std::size_t>(K_ALIGN_FRAMES); ++frame)
    {
        ASSERT_EQ(ears[0][frame], 0.0f) << "frame " << frame;
        ASSERT_EQ(ears[1][frame], 0.0f) << "frame " << frame;
    }
    double peak_level = 0.0;
    for (std::size_t frame = 0; frame < reference_left.size(); ++frame)
    {
        peak_level = std::max(peak_level, static_cast<double>(std::fabs(reference_left[frame])));
    }
    EXPECT_GT(peak_level, 0.1);
    EXPECT_LT(peak_difference(ears[0], reference_left, 0, reference_left.size()),
              1e-5 * peak_level);
    EXPECT_LT(peak_difference(ears[1], reference_right, 0, reference_right.size()),
              1e-5 * peak_level);
    EXPECT_LT(peak_difference(ears[0], ears[1], 0, 1500), 1e-4 * peak_level);
    EXPECT_GE(onset(ears[0], 1500, 4096, 1e-3), 1540);
    EXPECT_LE(onset(ears[0], 1500, 4096, 1e-3), 1560);
}

TEST(Medium_RoomNode, RoomNodeDefaultsDirectDelayToCompensated)
{
    // Physical timing adds round(d / c * fs) frames. See tests/nodes/room/test_room_node/README.md.
    SushiDSP::room::RoomNode node;
    node.prepare(K_INFO);
    EXPECT_FLOAT_EQ(node.get_parameter("direct_delay"), K_DIRECT_COMPENSATED);
    const std::uint32_t compensated = node.latency_samples();
    node.set_parameter_for_control_thread("direct_delay", K_DIRECT_PHYSICAL);
    const std::uint32_t physical = node.latency_samples();
    EXPECT_EQ(compensated, K_ALIGN_FRAMES);
    EXPECT_EQ(physical - compensated, 363u);

    std::string reason;
    ASSERT_TRUE(publish_scene(node, centred_dead_room(), reason)) << reason;
    EXPECT_EQ(node.latency_samples(), K_ALIGN_FRAMES + 280u);
    node.set_parameter_for_control_thread("direct_delay", K_DIRECT_COMPENSATED);
    EXPECT_EQ(node.latency_samples(), K_ALIGN_FRAMES);

    // Before prepare() there is no set and nothing to report.
    SushiDSP::room::RoomNode fresh;
    EXPECT_EQ(fresh.latency_samples(), 0u);
}

TEST(Medium_RoomNode, RoomNodeDirectOnsetMovesWithTheDirectDelayChoice)
{
    // The choice shifts the whole render by 280 frames. See tests/nodes/room/test_room_node/README.md.
    const SushiDSP::room::RoomScene scene = centred_dead_room();
    std::vector<float> impulse(4096, 0.0f);
    impulse[0] = 1.0f;
    std::unique_ptr<SushiDSP::room::RoomNode> node = prepared_node(scene, K_LISTENER_EARS);
    const std::vector<float> compensated = render(*node, impulse);
    node->set_parameter_for_control_thread("direct_delay", K_DIRECT_PHYSICAL);
    node->reset();
    const std::vector<float> physical = render(*node, impulse);

    const long compensated_direct = onset(compensated, 0, 1500, 1e-4);
    const long physical_direct = onset(physical, 0, 1500, 1e-4);
    ASSERT_GE(compensated_direct, 0);
    ASSERT_GE(physical_direct, 0);
    EXPECT_EQ(physical_direct - compensated_direct, 280);
    EXPECT_NEAR(static_cast<double>(compensated_direct), compensated_direct_frames(2.0), 3.0);
    const long compensated_floor = onset(compensated, 1500, 4096, 1e-3);
    const long physical_floor = onset(physical, 1500 + 280, 4096, 1e-3);
    ASSERT_GE(compensated_floor, 0);
    ASSERT_GE(physical_floor, 0);
    EXPECT_EQ(physical_floor - compensated_floor, 280);

    node->set_parameter_for_audio_thread(std::uint16_t{0}, K_DIRECT_COMPENSATED);
    EXPECT_FLOAT_EQ(node->get_parameter("direct_delay"), K_DIRECT_COMPENSATED);
    node->reset();
    EXPECT_EQ(onset(render(*node, impulse), 0, 1500, 1e-4), physical_direct);
    std::string reason;
    ASSERT_TRUE(publish_scene(*node, scene, reason)) << reason;
    node->reset();
    EXPECT_EQ(onset(render(*node, impulse), 0, 1500, 1e-4), compensated_direct);
}

TEST(Medium_RoomNode, RoomNodeReproducesWoodworthsInterauralDelayOnTheFallback)
{
    // Woodworth's delay is (a / c)(theta + sin theta). See tests/nodes/room/test_room_node/README.md.
    const std::array<double, 3> degrees = {30.0, 60.0, 90.0};
    const std::array<double, 3> expected_ms = {0.2611, 0.4881, 0.6558};
    std::vector<float> impulse(2048, 0.0f);
    impulse[0] = 1.0f;
    for (std::size_t index = 0; index < degrees.size(); ++index)
    {
        const double theta = degrees[index] * K_PI / 180.0;
        SushiDSP::room::RoomScene scene = centred_dead_room();
        scene.listener_position = {8.0, 6.0, 6.0};
        scene.listener_yaw_rad = K_PI;
        scene.source_position = {8.0 - 2.0 * std::cos(theta), 6.0 - 2.0 * std::sin(theta), 6.0};
        scene.source_yaw_rad = theta;
        const std::array<std::vector<float>, 2> ears =
            render_both(*prepared_node(scene, K_LISTENER_EARS), impulse);
        const double lag = interaural_lag(ears[0], ears[1], 0, 900, 60);
        const double woodworth_s =
            SushiDSP::room::SphericalHeadHrtf::interaural_time_difference(theta);
        RecordProperty(("lag_ms_" + std::to_string(static_cast<int>(degrees[index]))).c_str(),
                       lag / K_SAMPLE_RATE * 1000.0);
        EXPECT_NEAR(woodworth_s * 1000.0, expected_ms[index], 5e-4) << degrees[index];
        EXPECT_NEAR(lag / K_SAMPLE_RATE, woodworth_s, 20e-6) << degrees[index] << " degrees";
    }
}

TEST(Medium_RoomNode, RoomNodeReportsSphericalHeadWithNoAssetAndMeasuredWithOne)
{
    // An empty HRTF set restores the spherical head. See tests/nodes/room/test_room_node/README.md.
    std::unique_ptr<SushiDSP::room::RoomNode> node =
        prepared_node(centred_dead_room(), K_LISTENER_EARS);
    EXPECT_EQ(node->hrtf_source(), SushiDSP::room::HrtfSource::SphericalHead);

    SushiDSP::room::HrtfSet fixture;
    std::string error;
    ASSERT_TRUE(
        SushiDSP::room::HrtfSetLoader::load(fixture_set_path(), K_SAMPLE_RATE, fixture, error))
        << error;
    ASSERT_EQ(fixture.direction_count(), 6u);
    ASSERT_TRUE(node->set_hrtf(std::move(fixture)));
    EXPECT_EQ(node->hrtf_source(), SushiDSP::room::HrtfSource::Measured);
    std::vector<float> impulse(2048, 0.0f);
    impulse[0] = 1.0f;
    for (const float sample : render(*node, impulse))
    {
        ASSERT_TRUE(std::isfinite(sample));
    }

    ASSERT_TRUE(node->set_hrtf(SushiDSP::room::HrtfSet{}));
    EXPECT_EQ(node->hrtf_source(), SushiDSP::room::HrtfSource::SphericalHead);

    // A set without a third-order decoder is refused whole and the source does not move.
    SushiDSP::room::HrtfSet bare;
    ASSERT_TRUE(SushiDSP::room::HrtfSetLoader::load(fixture_set_path(), K_SAMPLE_RATE, bare, error))
        << error;
    bare.set_decoder(1, std::vector<float>(2 * 4 * SushiDSP::room::HrtfSet::K_DECODER_TAPS));
    EXPECT_FALSE(node->set_hrtf(std::move(bare)));
    EXPECT_EQ(node->hrtf_source(), SushiDSP::room::HrtfSource::SphericalHead);
}

TEST(Medium_RoomNode, RoomNodeTailIsCoherentLowAndIncoherentHigh)
{
    // A window 1024 frames after t_c holds the tail alone. See tests/nodes/room/test_room_node/README.md.
    const SushiDSP::room::RoomScene scene = centred_dead_room();
    std::vector<float> impulse(32768, 0.0f);
    impulse[0] = 1.0f;
    const std::array<std::vector<float>, 2> ears =
        render_both(*prepared_node(scene, K_LISTENER_EARS), impulse);
    const std::size_t begin = 8397 - 280 + K_ALIGN_FRAMES + 1024;
    const std::size_t end = begin + 16384;
    const double low = band_iacc(ears[0], ears[1], begin, end, 125.0);
    const double high = band_iacc(ears[0], ears[1], begin, end, 4000.0);
    RecordProperty("iacc_125_hz", low);
    RecordProperty("iacc_4_khz", high);
    EXPECT_GT(energy(ears[0], begin, end), 0.0);
    EXPECT_GT(low, 0.8);
    EXPECT_LT(high, 0.3);
}

namespace
{
    /// @brief Bounds the decoded near ear's third-octave error against the measured set, in dB.
    constexpr double K_NEAR_EAR_TOLERANCE_DB = 3.0;

    /// @brief Names the lowest third octave the near ear's wider bound applies from, in hertz.
    constexpr double K_NEAR_EAR_HIGH_BAND_HZ = 5000.0;

    /// @brief Bounds the decoded near ear's third-octave error from K_NEAR_EAR_HIGH_BAND_HZ up, in dB.
    constexpr double K_NEAR_EAR_HIGH_TOLERANCE_DB = 6.0;

    /// @brief Bounds the rendered rear notch's depth against the set's own, in dB.
    constexpr double K_NOTCH_TOLERANCE_DB = 1.0;

    /// @brief Returns the depth of the notch between the 5 and 12.5 kHz third octaves, in dB:
    /// their mean less the lowest of the 6.3, 8 and 10 kHz third octaves.
    double rear_notch_depth_db(const std::vector<double>& power_db)
    {
        double floor_db = 1e9;
        for (const double band : {6300.0, 8000.0, 10000.0})
        {
            floor_db = std::min(floor_db, third_octave_db(power_db, band));
        }
        return 0.5 * (third_octave_db(power_db, 5000.0) + third_octave_db(power_db, 12500.0)) -
               floor_db;
    }

    /// @brief Bounds the decoded far ear's third-octave error against the measured set, in dB.
    constexpr double K_FAR_EAR_TOLERANCE_DB = 10.0;

    /// @brief Holds the |sin(azimuth)| under which a direction counts as in the median plane.
    constexpr double K_MEDIAN_PLANE_SINE = 0.1;
} // namespace

TEST(Medium_RoomNode, RoomNodeDecoderMagnitudeMatchesTheMeasuredSet)
{
    // The case runs only with the SADIE II set present. See tests/nodes/room/test_room_node/README.md.
    SushiDSP::room::HrtfSet set;
    std::string error;
    if (!SushiDSP::room::HrtfSetLoader::load(measured_set_path(), K_SAMPLE_RATE, set, error))
    {
        GTEST_SKIP() << "no measured set at " << measured_set_path() << ": " << error;
    }
    SushiDSP::room::BinauralDecoder decoder;
    decoder.prepare(set, K_SAMPLE_RATE, K_BLOCK);
    for (const double elevation_deg : {-30.0, 0.0, 30.0})
    {
        for (int step = 0; step < 8; ++step)
        {
            const double azimuth = step * K_PI / 4.0;
            const double elevation = elevation_deg * K_PI / 180.0;
            const std::array<std::vector<float>, 2> decoded =
                decode_plane_wave(decoder, azimuth, elevation, 1024);
            const SushiDSP::room::HrtfSet::Pair pair = set.interpolate(azimuth, elevation);
            for (std::uint32_t ear = 0; ear < 2; ++ear)
            {
                const std::vector<double> decoded_db =
                    windowed_power_db(decoded[ear], 0, decoded[ear].size());
                const std::vector<float> hrir(ear == 0 ? pair.left.begin() : pair.right.begin(),
                                              ear == 0 ? pair.left.end() : pair.right.end());
                const std::vector<double> measured_db = windowed_power_db(hrir, 0, hrir.size());
                const double side = std::sin(azimuth);
                const bool far_ear =
                    ear == 0 ? side < -K_MEDIAN_PLANE_SINE : side > K_MEDIAN_PLANE_SINE;
                for (const double band : K_DECODER_BANDS)
                {
                    const double tolerance_db = far_ear ? K_FAR_EAR_TOLERANCE_DB
                                                : band >= K_NEAR_EAR_HIGH_BAND_HZ
                                                    ? K_NEAR_EAR_HIGH_TOLERANCE_DB
                                                    : K_NEAR_EAR_TOLERANCE_DB;
                    EXPECT_NEAR(third_octave_db(decoded_db, band),
                                third_octave_db(measured_db, band), tolerance_db)
                        << "azimuth " << step * 45 << " elevation " << elevation_deg << " ear "
                        << ear << " band " << band;
                }
            }
        }
    }
}

TEST(Medium_RoomNode, RoomNodeCarriesTheRearNotchWithAMeasuredSet)
{
    // The rear notch is within 1 dB of the set's own. See tests/nodes/room/test_room_node/README.md.
    SushiDSP::room::HrtfSet set;
    std::string error;
    if (!SushiDSP::room::HrtfSetLoader::load(measured_set_path(), K_SAMPLE_RATE, set, error))
    {
        GTEST_SKIP() << "no measured set at " << measured_set_path() << ": " << error;
    }
    const SushiDSP::room::HrtfSet::Pair rear = set.interpolate(K_PI, 0.0);
    const std::vector<float> rear_left(rear.left.begin(), rear.left.end());
    const double measured_depth_db =
        rear_notch_depth_db(windowed_power_db(rear_left, 0, rear_left.size()));
    std::unique_ptr<SushiDSP::room::RoomNode> node =
        prepared_node(centred_dead_room(), K_LISTENER_EARS);
    ASSERT_TRUE(node->set_hrtf(std::move(set)));
    ASSERT_EQ(node->hrtf_source(), SushiDSP::room::HrtfSource::Measured);
    std::vector<float> impulse(8 * K_BLOCK, 0.0f);
    impulse[0] = 1.0f;
    const double rendered_depth_db =
        rear_notch_depth_db(windowed_power_db(render(*node, impulse), 0, K_DIRECT_WINDOW));
    RecordProperty("notch_depth_db", rendered_depth_db);
    EXPECT_NEAR(rendered_depth_db, measured_depth_db, K_NOTCH_TOLERANCE_DB);
}
