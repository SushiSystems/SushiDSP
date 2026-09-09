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
 * @brief The room node: one cabinet in one shoebox, the standing waves, the taps, the tail and the swap.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "SushiDSP/core/IAudioNode.hpp"
#include "SushiDSP/core/NodeDescriptor.hpp"
#include "SushiDSP/core/RtSwapSlot.hpp"
#include "SushiDSP/math/SmoothedParameter.hpp"
#include "SushiDSP/room/CrossoverPair.hpp"
#include "SushiDSP/room/EarlyReflectionRenderer.hpp"
#include "SushiDSP/room/EarlyTapSet.hpp"
#include "SushiDSP/room/HrtfSet.hpp"
#include "SushiDSP/room/IRoomHost.hpp"
#include "SushiDSP/room/LateReverb.hpp"
#include "SushiDSP/room/ModalBank.hpp"
#include "SushiDSP/room/ModalDesign.hpp"
#include "SushiDSP/room/RoomRenderSet.hpp"
#include "SushiDSP/room/RoomScene.hpp"

namespace SushiDSP::room
{
    /// @brief One cabinet in one room, heard from one spot: PortLayout{1, 2}. Owns the seam's
    /// crossover, the modal banks, the early renderer, the tail and the render-set slot; every
    /// set is built on the control thread and crossfaded in on the audio thread. Until the
    /// binaural path lands, every set is rendered for the microphone listener. Spec §1, §5.2,
    /// §8 and §9a.
    class RoomNode final : public IAudioNode, public IRoomHost
    {
    public:
        /// @brief Counts the frames a newly acquired set fades in over, raised cosine.
        static constexpr std::uint32_t kCrossfadeFrames = 512;

        /// @brief Puts every parameter at its descriptor default and the default scene in place.
        RoomNode();

        /// @brief Sizes the input ring, the tail's lines, the modal banks and the block buffers,
        /// builds the spherical-head fallback and publishes a first set for the scene in place.
        void prepare(const PrepareInfo& info) override;

        /// @brief Renders one block: one acquire, the split at the seam, the early taps, the
        /// tail, the standing waves, the mix and the level.
        void process(ProcessContext& context) override;

        /// @brief Returns mono in, two channels out.
        PortLayout ports() const override;

        /// @brief Reads a parameter by id; an unknown id reads 0.
        float get_parameter(const char* id) const override;

        /// @brief Writes a parameter by id. Control thread only.
        void set_parameter(const char* id, float value) override;

        /// @brief Writes a parameter by descriptor index; stores a target and nothing else.
        void set_parameter(std::uint16_t index, float value) override;

        /// @brief Silences the ring, every state bank, the tail, the crossover and the crossfade.
        void reset() override;

        /// @brief Returns 0 until the binaural path adds its partition latency.
        std::uint32_t latency_samples() const override;

        /// @brief Returns "sushidsp.room".
        const char* type_id() const override;

        bool       set_scene(const RoomScene& scene, std::string& reason_out) override;
        RoomScene  scene() const override;
        bool       set_hrtf(HrtfSet set) override;
        HrtfSource hrtf_source() const override;
        double     schroeder_frequency_hz() const override;
        void       reclaim_rooms() override;

        /// @brief Drops tap 0, the (0, 0, 0) image, from a set built for the ears, whose direct
        /// sound the HRIR pair renders; a microphone set keeps it, its direct sound being a tap.
        /// @pre The taps are sorted by arrival, so tap 0 is the direct image whenever count > 0.
        static void remove_direct_image(RoomRenderSet& set) noexcept;

        /// @brief Returns true when two designs hold the same resonators: the same count and
        /// every paired section equal bit for bit, as one room designs them whatever the poses.
        static bool same_resonators(const ModalDesign& retiring,
                                    const ModalDesign& arriving) noexcept;

    private:
        /// @brief Counts the channels the early renderer accumulates into.
        static constexpr std::uint32_t kChannels = EarlyReflectionRenderer::kOutputChannels;

        /// @brief Counts the state banks, one per side of a crossfade.
        static constexpr std::uint32_t kBanks = EarlyReflectionRenderer::kBankCount;

        /// @brief Counts the ears the seam is split for.
        static constexpr std::uint32_t kEars = 2;

        /// @brief Holds the frames of one block a crossfade covers and how far the fade had
        /// run when the block began.
        struct FadeWindow
        {
            /// @brief Counts the fade frames that ran before this block.
            std::uint32_t done;

            /// @brief Counts the frames of this block still inside the fade.
            std::uint32_t fading;
        };

        /// @brief Builds a set for the scene and HRTF in place, publishes it and records the
        /// frequency the set splits at.
        /// @return False and fills reason_out when the build refuses the scene or the set.
        bool publish(std::string& reason_out);

        /// @brief Copies a newly acquired set's taps into the spare bank, zeroes that bank's
        /// state, carries the active bank's state into every tap the arriving set keeps, hands
        /// the modal design to adopt_modal, places the crossover and starts the fade; the first
        /// set ever is copied in from zero state with no fade. Taps pair by index: an arriving
        /// tap past the retiring count starts from zero, a retiring tap past the arriving count
        /// is dropped. Spec §5.2.
        void adopt(const RoomRenderSet& set) noexcept;

        /// @brief Adopts a set's modal design under the match rule: resonators identical to the
        /// active bank's keep that bank's state and ramp its gains over the fade; resonators
        /// that differ start the spare bank from zero and blend the two over the fade.
        void adopt_modal(const ModalDesign& design) noexcept;

        /// @brief Renders the active bank, and the retiring bank blended under a raised cosine
        /// while a fade runs, into the active accumulation buffers.
        void render_early(std::uint32_t frames, const FadeWindow& window) noexcept;

        /// @brief Splits the input at the seam into modal_low_ and geometric_high_; a bypassed
        /// crossover passes everything to the high half.
        void split_input(const float* input, std::uint32_t frames) noexcept;

        /// @brief Runs the low half through the active modal bank, blending the retiring bank
        /// or ramping the gains under the fade, into modal_left_ and modal_right_.
        void render_modal(std::uint32_t frames, const FadeWindow& window) noexcept;

        /// @brief Runs the active bank over one frame with its gains at the fade's weight
        /// between the retiring and the arriving design.
        void render_modal_ramp(std::uint32_t frame, float weight) noexcept;

        /// @brief Sums the early channel 0 and the tail into wet_, the geometric field.
        void sum_wet(std::uint32_t frames) noexcept;

        /// @brief Adds each ear's modal field to the geometric sum, into ear_.
        void join_ears(std::uint32_t frames) noexcept;

        /// @brief Writes the smoothed dry-wet mix at the smoothed level to both outputs.
        void write_outputs(const float* input, float* const* outputs,
                           std::uint32_t frames) noexcept;

        /// @brief Zeroes one accumulation set over the block.
        void clear_accumulation(std::uint32_t set, std::uint32_t frames) noexcept;

        /// @brief Stores the rate prepare() ran at, in hertz.
        double sample_rate_ = 48000.0;

        /// @brief Bounds the block length prepare() sized the buffers for.
        std::uint32_t max_block_ = 0;

        /// @brief Records that prepare() has run.
        bool prepared_ = false;

        /// @brief Stores the scene in place; control thread.
        RoomScene scene_;

        /// @brief Stores the HRTF set in place; control thread.
        HrtfSet hrtf_;

        /// @brief Names where hrtf_ came from.
        HrtfSource hrtf_source_ = HrtfSource::SphericalHead;

        /// @brief Stores the frequency the published set splits at, in hertz: the solver's
        /// crossover while the modal layer runs, Eyring's Schroeder frequency otherwise.
        double schroeder_hz_ = 0.0;

        /// @brief Smooths the dry-wet mix, 0 dry to 1 wet.
        SmoothedParameter mix_{ 1.0f };

        /// @brief Smooths the output level as a linear gain.
        SmoothedParameter level_{ 1.0f };

        /// @brief Stores the level parameter in decibels, what get_parameter reads back.
        float level_db_ = 0.0f;

        /// @brief Stores the listener choice, 0 ears and 1 microphone; applied by a later phase.
        float listener_ = 0.0f;

        /// @brief Stores the direct-delay choice, 0 compensated and 1 physical; applied later.
        float direct_delay_ = 0.0f;

        /// @brief Carries each published set to the audio thread.
        RtSwapSlot<RoomRenderSet> slot_;

        /// @brief Identifies the set adopted last; compared, never dereferenced past its block.
        const RoomRenderSet* adopted_ = nullptr;

        /// @brief Reads the taps out of the input ring.
        EarlyReflectionRenderer early_;

        /// @brief Renders the tail.
        LateReverb late_;

        /// @brief Stores the taps each state bank renders; the retiring set is read from here,
        /// never from a slot pointer, so a reclaim can free it at any time.
        std::array<EarlyTapSet, kBanks> bank_taps_{};

        /// @brief Indexes the bank rendering the set in place.
        std::uint32_t active_bank_ = 0;

        /// @brief Counts the crossfade frames left; zero when no fade runs.
        std::uint32_t fade_remaining_ = 0;

        /// @brief Stores kBanks sets of kChannels accumulation buffers of max_block_ frames.
        std::vector<float> accumulation_;

        /// @brief Points at each accumulation set's kChannels buffers.
        std::array<std::array<float*, kChannels>, kBanks> accumulation_pointers_{};

        /// @brief Stores the tail's block.
        std::vector<float> tail_;

        /// @brief Stores the summed geometric block.
        std::vector<float> wet_;

        /// @brief Splits the input at the seam: the low half feeds the modal bank, the high half
        /// the early renderer and the tail.
        CrossoverPair input_crossover_;

        /// @brief Renders the standing waves, one bank per side of a crossfade.
        std::array<ModalBank, kBanks> modal_banks_;

        /// @brief Stores the design each modal bank renders; copied in like bank_taps_.
        std::array<ModalDesign, kBanks> bank_modes_{};

        /// @brief Stores the gains the active bank ramps from, the design rendered last.
        ModalDesign modal_from_{};

        /// @brief Stores the active bank's resonators with the gains at the ramp's step.
        ModalDesign modal_blend_{};

        /// @brief Indexes the modal bank rendering the set in place.
        std::uint32_t active_modal_bank_ = 0;

        /// @brief Records that the retiring modal bank blends out under the running fade.
        bool modal_fading_ = false;

        /// @brief Records that the active modal bank's gains ramp under the running fade.
        bool modal_ramping_ = false;

        /// @brief Stores the input's low half, the modal bank's feed.
        std::vector<float> modal_low_;

        /// @brief Stores the input's high half, the geometric layers' feed.
        std::vector<float> geometric_high_;

        /// @brief Stores the modal field at the left ear.
        std::vector<float> modal_left_;

        /// @brief Stores the modal field at the right ear.
        std::vector<float> modal_right_;

        /// @brief Stores the retiring modal bank's left ear while it fades out.
        std::vector<float> retiring_left_;

        /// @brief Stores the retiring modal bank's right ear while it fades out.
        std::vector<float> retiring_right_;

        /// @brief Stores kEars blocks of the joined wet field, one per ear.
        std::vector<float> ear_;

        /// @brief Points at each ear's block of ear_.
        std::array<float*, kEars> ear_pointers_{};
    };

    /// @brief Returns the registration entry for "sushidsp.room".
    const NodeDescriptor& room_descriptor();
}
