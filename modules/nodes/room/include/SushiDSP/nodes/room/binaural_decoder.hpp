/**************************************************************************/
/* binaural_decoder.hpp                                                   */
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
 * @file binaural_decoder.hpp
 * @brief Thirty-two partitioned convolutions turning sixteen Ambisonic channels into two ears.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <cstdint>
#include <vector>

#include <SushiDSP/foundation/math/partitioned_convolution.hpp>
#include <SushiDSP/nodes/room/ambisonic_encoder.hpp>
#include <SushiDSP/nodes/room/hrtf_set.hpp>

namespace SushiDSP::room
{
    /// @brief Runs 32 partitioned convolutions turning 16 Ambisonic channels into two ears. Its
    /// cost is fixed by the order and does not depend on the scene. Audio thread. Spec §9.4.
    class BinauralDecoder
    {
        public:
            /// @brief Counts the ears.
            static constexpr std::uint32_t K_EARS = 2;

            /// @brief Counts the Ambisonic input channels.
            static constexpr std::uint32_t K_CHANNELS = AmbisonicEncoder::K_CHANNELS;

            /// @brief Builds an unprepared decoder that reports ready() false.
            BinauralDecoder();

            /// @brief Builds the 32 convolvers from the set's decoder filters. Control thread.
            /// @param max_block_frames Largest frames value process() will ever be called with.
            /// @throw std::runtime_error when the set carries no decoder of the expected order.
            void prepare(const HrtfSet& set, double sample_rate, std::uint32_t max_block_frames);

            /// @brief Reports whether prepare() has succeeded.
            bool ready() const noexcept;

            /// @brief Returns the delay from input to output in frames: one partition.
            std::uint32_t latency_frames() const noexcept;

            /// @brief Decodes one block; inputs is K_CHANNELS channel pointers of frames samples each.
            /// @pre ready(), frames <= max_block_frames, and left and right alias no input.
            void process(const float* const* inputs, float* left, float* right,
                         std::uint32_t frames) noexcept;

            /// @brief Clears the convolvers' history and the re-blocking buffers.
            void reset() noexcept;

        private:
            /// @brief Runs the 32 convolvers on one full input partition into the output partition.
            void decode_partition() noexcept;

            /// @brief Stores the partition size in frames; zero until prepared.
            std::uint32_t partition_size_;

            /// @brief Stores the frames gathered so far in the input partition.
            std::uint32_t fill_;

            /// @brief Stores the convolvers, ear-major then ACN order: ear * K_CHANNELS + channel.
            std::vector<PartitionedConvolution> convolvers_;

            /// @brief Stores the input partition, channel-major, K_CHANNELS times partition_size_.
            std::vector<float> input_partition_;

            /// @brief Stores the decoded partition being drained, ear-major, K_EARS times
            /// partition_size_.
            std::vector<float> output_partition_;

            /// @brief Stores one convolver's output before it is summed into an ear.
            std::vector<float> scratch_;
    };
} // namespace SushiDSP::room
