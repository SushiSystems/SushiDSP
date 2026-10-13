/**************************************************************************/
/* receiver_output.hpp                                                    */
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
 * @file receiver_output.hpp
 * @brief Declares the head the listener hears through and the stages that sum a receiver's pairs.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#include "SushiDSP/core/rt_swap_slot.hpp"
#include "SushiDSP/room/binaural_decoder.hpp"
#include "SushiDSP/room/fade_window.hpp"
#include "SushiDSP/room/fir_filter.hpp"
#include "SushiDSP/room/frame_delay.hpp"
#include "SushiDSP/room/hrtf_set.hpp"
#include "SushiDSP/room/pair_stage.hpp"
#include "SushiDSP/room/room_host.hpp"

namespace SushiDSP::Room
{
    /// @brief Owns the head the listener hears the room through: the HRTF set and the decoder
    /// published for it. The set and its source are control-thread state; the decoder reaches the
    /// audio thread through a swap slot.
    class HeadHolder
    {
        public:
            /// @brief Returns the HRTF set in place. Control thread.
            /// @return The set.
            const room::HrtfSet& hrtf() const noexcept;

            /// @brief Puts an HRTF set in place. Control thread.
            /// @param set The set to hold.
            void set_hrtf(room::HrtfSet set);

            /// @brief Moves the HRTF set out, leaving an empty one behind. Control thread.
            /// @return The set that was in place.
            room::HrtfSet take_hrtf() noexcept;

            /// @brief Names where the set in place came from.
            /// @return The source.
            room::HrtfSource source() const noexcept;

            /// @brief Records where the set in place came from.
            /// @param source The source.
            void set_source(room::HrtfSource source) noexcept;

            /// @brief Drops a measured set the rate no longer fits and builds the spherical-head
            /// fallback when that is what the source names.
            /// @param sample_rate The rate the node runs at, in hertz.
            void refresh_head(double sample_rate);

            /// @brief Builds a decoder from the set in place and publishes it.
            /// @param sample_rate The rate the node runs at, in hertz.
            /// @param max_block_frames The longest block.
            /// @throw std::runtime_error when the set carries no third-order decoder.
            void publish_decoder(double sample_rate, std::uint32_t max_block_frames);

            /// @brief Publishes a decoder built elsewhere.
            /// @param decoder The decoder, already prepared.
            void publish_decoder(std::unique_ptr<room::BinauralDecoder> decoder);

            /// @brief Returns the decoder in place; one atomic acquire, audio thread.
            /// @return The decoder, or nullptr before the first publish.
            room::BinauralDecoder* acquire_decoder() noexcept;

            /// @brief Clears the published decoder's history. Control thread, never while the
            /// audio thread runs.
            void reset_decoder() noexcept;

            /// @brief Frees every decoder the audio thread has moved past. Control thread.
            void reclaim();

        private:
            /// @brief Stores the HRTF set in place.
            room::HrtfSet hrtf_;

            /// @brief Names where hrtf_ came from.
            room::HrtfSource source_ = room::HrtfSource::SphericalHead;

            /// @brief Carries each published decoder to the audio thread.
            RtSwapSlot<room::BinauralDecoder> decoder_slot_;
    };

    /// @brief Sums every cabinet's pair at the listener and presents it at the two ears: one
    /// decode of the summed ambisonic channels, the summed tail through the diffuse-field pair,
    /// and the tail and the modal field held back to the decoder's partition. Audio thread.
    class EarsOutput
    {
        public:
            /// @brief Sizes the diffuse pair, both alignment delays and every block buffer.
            /// @param alignment_frames The decoder's partition.
            /// @param max_block_frames The longest block.
            void prepare(std::uint32_t alignment_frames, std::uint32_t max_block_frames);

            /// @brief Zeroes the diffuse pair and both alignment delays.
            void reset() noexcept;

            /// @brief Copies a head's diffuse-field pair aside until the fade is due to move it.
            /// @param left The left ear's K_TAPS coefficients.
            /// @param right The right ear's K_TAPS coefficients.
            void stage_diffuse(const float* left, const float* right) noexcept;

            /// @brief Loads the staged diffuse-field pair once the fade has run its lag.
            /// @param window The block's fade window.
            void apply_pending_when_due(const room::FadeWindow& window) noexcept;

            /// @brief Starts a block with nothing summed.
            void begin() noexcept;

            /// @brief Adds one cabinet's pair and its modal field at the two ears.
            /// @param pair The cabinet's ears pair, already rendered.
            /// @param modal_left The cabinet's modal field at the left ear.
            /// @param modal_right The cabinet's modal field at the right ear.
            /// @param frames The block length.
            void add(const PairStage& pair, const float* modal_left, const float* modal_right,
                     std::uint32_t frames) noexcept;

            /// @brief Decodes, aligns and sums the block into the two ears.
            /// @param decoder The decoder in place, or nullptr.
            /// @param left Receives the left ear.
            /// @param right Receives the right ear.
            /// @param frames The block length.
            void finish(room::BinauralDecoder* decoder, float* left, float* right,
                        std::uint32_t frames) noexcept;

        private:
            /// @brief Counts the ambisonic channels one pair hands on.
            static constexpr std::uint32_t K_CHANNELS = PairStage::K_CHANNELS;

            /// @brief Adds a block into a sum, or copies it when it is the first of the block.
            void accumulate(float* sum, const float* block, std::uint32_t frames) const noexcept;

            /// @brief Bounds the block length prepare() sized the buffers for.
            std::uint32_t max_block_ = 0;

            /// @brief Counts the pairs added since begin().
            std::uint32_t added_ = 0;

            /// @brief Records that the decoder ran last block, so its history is current.
            bool decoder_fed_ = false;

            /// @brief Stores K_CHANNELS blocks of the summed ambisonic channels.
            std::vector<float> channels_;

            /// @brief Points at each summed channel.
            std::array<float*, K_CHANNELS> channel_pointers_{};

            /// @brief Stores the summed direct sound at each ear.
            std::array<std::vector<float>, room::K_EARS> direct_{};

            /// @brief Stores the summed tail at each ear, then its aligned presentation.
            std::array<std::vector<float>, room::K_EARS> tail_{};

            /// @brief Stores the tail through the diffuse pair and the alignment delay.
            std::array<std::vector<float>, room::K_EARS> tail_ears_{};

            /// @brief Stores the summed modal field at each ear, then its aligned presentation.
            std::array<std::vector<float>, room::K_EARS> modal_{};

            /// @brief Stores the decoded reflections at each ear.
            std::array<std::vector<float>, room::K_EARS> decoded_{};

            /// @brief Passes the tail through the diffuse-field average, one filter per ear.
            std::array<room::FirFilter, room::K_EARS> diffuse_{};

            /// @brief Delays the tail to the decoder's partition, one line per ear.
            std::array<room::FrameDelay, room::K_EARS> tail_delay_{};

            /// @brief Delays the modal field to the decoder's partition, one line per ear.
            std::array<room::FrameDelay, room::K_EARS> modal_delay_{};

            /// @brief Stores the diffuse pair waiting for the lagged fade to begin.
            std::array<std::array<float, room::FirFilter::K_TAPS>, room::K_EARS> pending_{};

            /// @brief Records that pending_ has not been loaded yet.
            bool pending_waiting_ = false;
    };

    /// @brief Sums every cabinet's pair at one microphone into one mono block and holds it back by
    /// the decoder's partition, so every port of the room lines up. Audio thread.
    class MicrophoneOutput
    {
        public:
            /// @brief Sizes the alignment delay and the block buffer.
            /// @param alignment_frames The decoder's partition.
            /// @param max_block_frames The longest block.
            void prepare(std::uint32_t alignment_frames, std::uint32_t max_block_frames);

            /// @brief Zeroes the alignment delay.
            void reset() noexcept;

            /// @brief Starts a block with nothing summed.
            void begin() noexcept;

            /// @brief Adds one cabinet's pair and its modal field at the microphone.
            /// @param pair The cabinet's microphone pair, already rendered.
            /// @param modal The cabinet's modal field at the microphone.
            /// @param frames The block length.
            void add(const PairStage& pair, const float* modal, std::uint32_t frames) noexcept;

            /// @brief Delays the summed block into the output.
            /// @param output Receives the microphone's block.
            /// @param frames The block length.
            void finish(float* output, std::uint32_t frames) noexcept;

        private:
            /// @brief Counts the pairs added since begin().
            std::uint32_t added_ = 0;

            /// @brief Stores the summed block.
            std::vector<float> sum_;

            /// @brief Delays the block to the decoder's partition.
            room::FrameDelay delay_;
    };
} // namespace SushiDSP::Room
