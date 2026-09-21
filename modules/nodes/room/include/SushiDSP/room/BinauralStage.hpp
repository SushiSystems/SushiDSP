/**************************************************************************/
/* BinauralStage.hpp                                                      */
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
 * @file BinauralStage.hpp
 * @brief Where the two halves meet: one head, its decoder, its alignment and the sum at each ear.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#include "SushiDSP/core/RtSwapSlot.hpp"
#include "SushiDSP/room/BinauralDecoder.hpp"
#include "SushiDSP/room/FadeWindow.hpp"
#include "SushiDSP/room/FirFilter.hpp"
#include "SushiDSP/room/FrameDelay.hpp"
#include "SushiDSP/room/HrtfSet.hpp"
#include "SushiDSP/room/IRoomHost.hpp"
#include "SushiDSP/room/RoomRenderSet.hpp"

namespace SushiDSP::room
{
    /// @brief Owns the head the whole room is heard through: the HRTF set, the decoder published
    /// for it, the diffuse-field pair the tail passes, the alignment delays the tail and the
    /// modal field wait at, and the sum each ear leaves by. Spec §9.4.
    class BinauralStage
    {
    public:
        /// @brief Holds what the other two paths hand the join, one block each.
        struct EarSources
        {
            /// @brief Points at the direct sound at each ear.
            std::array<const float*, kEars> direct;

            /// @brief Points at the tail block each ear reads.
            std::array<const float*, kEars> tail;

            /// @brief Points at the modal field at each ear.
            std::array<const float*, kEars> modal;

            /// @brief Points at the microphone's channel 0.
            const float* early_mono;
        };

        /// @brief Sizes the diffuse pair, both alignment delays and every block buffer.
        void prepare(std::uint32_t alignment_frames, std::uint32_t max_block_frames);

        /// @brief Zeroes the diffuse pair, both alignment delays and the published decoder.
        void reset() noexcept;

        /// @brief Returns the HRTF set in place. Control thread.
        const HrtfSet& hrtf() const noexcept;

        /// @brief Puts an HRTF set in place. Control thread.
        void set_hrtf(HrtfSet set);

        /// @brief Moves the HRTF set out, leaving an empty one behind. Control thread.
        HrtfSet take_hrtf() noexcept;

        /// @brief Names where the set in place came from.
        HrtfSource source() const noexcept;

        /// @brief Records where the set in place came from.
        void set_source(HrtfSource source) noexcept;

        /// @brief Drops a measured set the rate no longer fits and builds the spherical-head
        /// fallback when that is what the source names.
        void refresh_head(double sample_rate);

        /// @brief Builds a decoder from the set in place and publishes it.
        /// @throw std::runtime_error when the set carries no third-order decoder.
        void publish_decoder(double sample_rate, std::uint32_t max_block_frames);

        /// @brief Publishes a decoder built elsewhere.
        void publish_decoder(std::unique_ptr<BinauralDecoder> decoder);

        /// @brief Returns the decoder in place; one atomic acquire, audio thread.
        BinauralDecoder* acquire_decoder() noexcept;

        /// @brief Frees every decoder the audio thread has moved past. Control thread.
        void reclaim();

        /// @brief Copies a set's diffuse-field pair aside until the fade is due to move it.
        void stage_diffuse(const RoomRenderSet& set) noexcept;

        /// @brief Loads the staged diffuse-field pair into the two filters.
        void apply_pending() noexcept;

        /// @brief Decodes sixteen channels into the two ears, resetting the decoder first when
        /// it sat idle; a missing decoder writes zeros.
        void decode(BinauralDecoder* decoder, const float* const* channels,
                    std::uint32_t frames) noexcept;

        /// @brief Zeroes the decoded pair and marks the decoder idle.
        void clear_decoded(std::uint32_t frames) noexcept;

        /// @brief Passes the tail through the diffuse-field pair and the tail and the modal
        /// field through the alignment delay, the ears' presentations of both.
        void align(const EarSources& sources, std::uint32_t frames) noexcept;

        /// @brief Sums each ear: the ears' presentation, the microphone's, or the two weighted
        /// by the banks' sides of the fade when a fade crosses listeners.
        void join(const EarSources& sources, std::uint32_t frames, const FadeWindow& window,
                  bool active_ears, bool mixed) noexcept;

        /// @brief Zeroes both ears over every frame they hold.
        void silence() noexcept;

        /// @brief Returns one ear's joined wet field.
        const float* ear(std::uint32_t ear) const noexcept;

    private:
        /// @brief Holds the diffuse-field pair waiting for the lagged fade to begin.
        struct DiffuseAdoption
        {
            /// @brief Stores the left ear's diffuse-field average.
            std::array<float, FirFilter::kTaps> left;

            /// @brief Stores the right ear's diffuse-field average.
            std::array<float, FirFilter::kTaps> right;
        };

        /// @brief Stores the HRTF set in place; control thread.
        HrtfSet hrtf_;

        /// @brief Names where hrtf_ came from.
        HrtfSource hrtf_source_ = HrtfSource::SphericalHead;

        /// @brief Carries each published decoder to the audio thread.
        RtSwapSlot<BinauralDecoder> decoder_slot_;

        /// @brief Records that the decoder ran last block, so its history is current.
        bool decoder_fed_ = false;

        /// @brief Passes the ears' tail through the diffuse-field average, one filter per ear.
        std::array<FirFilter, kEars> diffuse_{};

        /// @brief Delays the ears' tail to the decoder's partition, one line per ear.
        std::array<FrameDelay, kEars> tail_delay_{};

        /// @brief Delays the ears' modal field to the decoder's partition, one line per ear.
        std::array<FrameDelay, kEars> modal_delay_{};

        /// @brief Stores the diffuse pair waiting for the lagged fade to begin.
        DiffuseAdoption pending_{};

        /// @brief Stores the ears' tail, through the diffuse pair and the alignment delay.
        std::array<std::vector<float>, kEars> tail_ears_{};

        /// @brief Stores the ears' modal field through the alignment delay.
        std::array<std::vector<float>, kEars> modal_ears_{};

        /// @brief Stores the ears' decoded reflections, left.
        std::vector<float> decoded_left_;

        /// @brief Stores the ears' decoded reflections, right.
        std::vector<float> decoded_right_;

        /// @brief Stores kEars blocks of the joined wet field, one per ear.
        std::vector<float> ear_;

        /// @brief Points at each ear's block of ear_.
        std::array<float*, kEars> ear_pointers_{};
    };
}
