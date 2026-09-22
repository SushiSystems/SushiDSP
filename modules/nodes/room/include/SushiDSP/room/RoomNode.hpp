/**************************************************************************/
/* RoomNode.hpp                                                           */
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
 * @file RoomNode.hpp
 * @brief The room node: one cabinet in one shoebox, the seam, the three paths and the swap.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

#include "SushiDSP/core/IAudioNode.hpp"
#include "SushiDSP/core/NodeDescriptor.hpp"
#include "SushiDSP/core/ParameterTable.hpp"
#include "SushiDSP/core/RtSwapSlot.hpp"
#include "SushiDSP/math/SmoothedParameter.hpp"
#include "SushiDSP/room/BinauralStage.hpp"
#include "SushiDSP/room/CrossoverPair.hpp"
#include "SushiDSP/room/DelayRing.hpp"
#include "SushiDSP/room/EarlyTapSet.hpp"
#include "SushiDSP/room/FadeWindow.hpp"
#include "SushiDSP/room/GeometricPath.hpp"
#include "SushiDSP/room/HrtfSet.hpp"
#include "SushiDSP/room/IRoomHost.hpp"
#include "SushiDSP/room/ModalDesign.hpp"
#include "SushiDSP/room/ModalPath.hpp"
#include "SushiDSP/room/RoomRenderSet.hpp"
#include "SushiDSP/room/RoomScene.hpp"

namespace SushiDSP::room
{
    /// @brief One cabinet in one room, heard from one spot: PortLayout{1, 2}. Splits the input at
    /// the seam, drives the modal path with the low half and the geometric path with the high
    /// half, and lets the binaural stage join them; every set is built on the control thread for
    /// the listener the parameter names and crossfaded in on the audio thread, one window read by
    /// all three. Spec §1, §5.2, §8, §9 and §9a.
    class RoomNode final : public IAudioNode, public IRoomHost
    {
    public:
        /// @brief Counts the frames a newly acquired set fades in over, raised cosine.
        static constexpr std::uint32_t kCrossfadeFrames = room::kCrossfadeFrames;

        /// @brief Names the crossfade window all three paths blend under.
        using FadeWindow = room::FadeWindow;

        /// @brief Puts every parameter at its descriptor default and the starter scene in place.
        RoomNode();

        /// @brief Sizes the input rings, the tail's lines, the modal banks, the alignment delays
        /// and the block buffers, builds the spherical-head fallback, publishes its decoder and a
        /// first set for the scene in place.
        void prepare(const PrepareInfo& info) override;

        /// @brief Renders one block: one acquire per slot, the split at the seam, the two paths,
        /// the join at the ears or the microphone, the mix and the level.
        void process(ProcessContext& context) override;

        /// @brief Returns mono in, two channels out.
        PortLayout ports() const override;

        /// @brief Reads a parameter by id; an unknown id reads 0.
        float get_parameter(const char* id) const override;

        /// @brief Writes a parameter by id. Control thread only. A changed listener or direct
        /// delay rebuilds the set through the path set_scene uses; the audio thread applies
        /// either choice only through the next published set.
        void set_parameter_for_control_thread(const char* id, float value) override;

        /// @brief Writes a parameter by descriptor index; stores a target and nothing else, so
        /// a listener or direct delay written here waits for the next publish.
        void set_parameter_for_audio_thread(std::uint16_t index, float value) override;

        /// @brief This node's parameters, one row each, feeding the descriptor and both
        /// dispatches. Declared on the class rather than at namespace scope so a row's
        /// lambdas can reach the private field a knob actually lives in.
        /// @return The table, built once on first use.
        static const ParameterTable<RoomNode, 4>& parameters();

        /// @brief Silences the three paths, the crossover and the crossfade.
        void reset() override;

        /// @brief Returns the ears' latency: one decoder partition, plus the direct path's
        /// propagation delay when direct_delay is physical; the microphone's is 0. Spec §9.4.
        std::uint32_t latency_samples() const override;

        /// @brief Returns "sushidsp.room".
        const char* type_id() const override;

        bool       set_scene(const RoomScene& scene, std::string& reason_out) override;
        RoomScene  scene() const override;
        bool       set_hrtf(HrtfSet set) override;
        HrtfSource hrtf_source() const override;
        double     schroeder_frequency_hz() const override;
        void       reclaim_rooms() override;

        /// @brief Copies a set's taps into a bank, as the geometric path does it. Audio thread.
        /// @pre The taps are sorted by arrival, so tap 0 is the direct image whenever count > 0.
        /// @return True when direct was written.
        static bool take_direct_image(const RoomRenderSet& set, EarlyTapSet& reflections,
                                      EarlyTap& direct) noexcept;

        /// @brief Moves an ears set's whole render earlier by its direct propagation delay, so
        /// the direct sound lands at the decoder's partition and the reflections and the tail
        /// keep their spacing to it: every tap but the first is floored at one frame, the tail's
        /// input delay at zero. A microphone set is left as it was. Control thread. Spec §9.4.
        static void compensate_direct_delay(RoomRenderSet& set) noexcept;

        /// @brief Returns true when two designs hold the same resonators, as the modal path
        /// judges it.
        static bool same_resonators(const ModalDesign& retiring,
                                    const ModalDesign& arriving) noexcept;

    private:
        /// @brief Returns the listener the parameter names.
        ListenerKind listener_kind() const noexcept;

        /// @brief Reports whether the direct_delay parameter names physical.
        bool direct_delay_physical() const noexcept;

        /// @brief Builds a set for the scene, HRTF, listener and direct delay in place,
        /// publishes it and records the frequency the set splits at and the latency it carries.
        /// @return False and fills reason_out when the build refuses the scene or the set.
        bool publish(std::string& reason_out);

        /// @brief Builds a decoder from the HRTF in place and publishes it.
        /// @throw std::runtime_error when the set carries no third-order decoder.
        void publish_decoder();

        /// @brief Places the crossover, stages what each path takes from a newly acquired set,
        /// hands the set to the two paths and starts the fade; the first set ever is taken in
        /// from zero state with no fade. Spec §5.2.
        void adopt(const RoomRenderSet& set) noexcept;

        /// @brief Lets both paths apply what they staged, once the fade has run its lag, so
        /// nothing at the output moves before the fade does.
        void apply_pending_when_due(const FadeWindow& window) noexcept;

        /// @brief Splits the input at the seam into the modal path's low half and the geometric
        /// path's high half; a bypassed crossover passes everything to the high half.
        void split_input(const float* input, std::uint32_t frames) noexcept;

        /// @brief Writes the smoothed dry-wet mix at the smoothed level to both outputs; the dry
        /// signal is delayed to the frame the wet output lands at, so the blend never runs the
        /// dry ahead of the wet.
        void write_outputs(const float* input, float* const* outputs,
                           std::uint32_t frames) noexcept;

        /// @brief Stores the rate prepare() ran at, in hertz.
        double sample_rate_ = 48000.0;

        /// @brief Bounds the block length prepare() sized the buffers for.
        std::uint32_t max_block_ = 0;

        /// @brief Stores the decoder's partition, the frames every other ears path waits.
        std::uint32_t alignment_frames_ = 0;

        /// @brief Records that prepare() has run.
        bool prepared_ = false;

        /// @brief Stores the scene in place; control thread.
        RoomScene scene_;

        /// @brief Stores the frequency the published set splits at, in hertz: the solver's
        /// crossover while the modal layer runs, Eyring's Schroeder frequency otherwise.
        double schroeder_hz_ = 0.0;

        /// @brief Smooths the dry-wet mix, 0 dry to 1 wet.
        SmoothedParameter mix_{ 1.0f };

        /// @brief Smooths the output level as a linear gain.
        SmoothedParameter level_{ 1.0f };

        /// @brief Stores the level parameter in decibels, what get_parameter reads back.
        float level_db_ = 0.0f;

        /// @brief Stores the listener choice, 0 ears and 1 microphone; written on either
        /// thread, read at publish.
        std::atomic<float> listener_{ 0.0f };

        /// @brief Names the listener the set published last was built for; control thread.
        ListenerKind published_listener_ = ListenerKind::Ears;

        /// @brief Stores the direct-delay choice, 0 compensated and 1 physical; written on
        /// either thread, read at publish.
        std::atomic<float> direct_delay_{ 0.0f };

        /// @brief Records that the set published last was built physical; control thread.
        bool published_physical_ = false;

        /// @brief Stores the propagation delay the set published last adds to the latency in
        /// frames: round(d / c * fs) when physical, zero otherwise; control thread.
        std::uint32_t published_direct_delay_frames_ = 0;

        /// @brief Carries each published set to the audio thread.
        RtSwapSlot<RoomRenderSet> slot_;

        /// @brief Identifies the set adopted last; compared, never dereferenced past its block.
        const RoomRenderSet* adopted_ = nullptr;

        /// @brief Splits the input at the seam: the low half feeds the modal path, the high half
        /// the geometric path.
        CrossoverPair input_crossover_;

        /// @brief Renders the standing waves below the seam.
        ModalPath modal_;

        /// @brief Renders the direct sound, the reflections and the tail above the seam.
        GeometricPath geometric_;

        /// @brief Joins the two halves at the ears or the microphone.
        BinauralStage binaural_;

        /// @brief Counts the crossfade frames left; zero when no fade runs.
        std::uint32_t fade_remaining_ = 0;

        /// @brief Counts the frames the running fade spans: kCrossfadeFrames plus its lag.
        std::uint32_t fade_total_ = 0;

        /// @brief Stores the lag the running fade is read at by the paths that blend at the
        /// output: the alignment delay when either bank is the ears, zero otherwise.
        std::uint32_t fade_lag_ = 0;

        /// @brief Records that the paths hold a staged adoption not applied yet.
        bool pending_ = false;

        /// @brief Delays the dry signal to the frame the wet ears output lands at.
        DelayRing dry_delay_;

        /// @brief Holds one block of the delayed dry signal, sized at prepare().
        std::vector<float> dry_delay_buffer_;

        /// @brief Stores the dry delay for the set adopted last, in frames: the alignment delay
        /// for the ears, zero for the microphone, which carries no decoder partition. Audio
        /// thread.
        std::uint32_t dry_delay_frames_ = 0;
    };

    /// @brief Returns the registration entry for "sushidsp.room".
    const NodeDescriptor& room_descriptor();
}
