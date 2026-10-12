/**************************************************************************/
/* streaming_convolution.hpp                                              */
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
 * @file streaming_convolution.hpp
 * @brief Declares the re-blocking front of PartitionedConvolution.
 * @author Mustafa Garip
 */

#pragma once
#include <cstdint>
#include <vector>
#include "SushiDSP/math/partitioned_convolution.hpp"
namespace SushiDSP
{
    /// @brief Re-blocks an arbitrary-length input stream onto a fixed-partition
    /// PartitionedConvolution. Costs exactly one partition of latency. Allocation-free after
    /// construction; process() accepts any frame count up to max_block.
    class StreamingConvolution
    {
        public:
            /// @param partition_size Internal FFT partition (power of two).
            /// @param impulse_response IR samples. @param impulse_length Sample count.
            /// @param max_block Largest frames value process() will ever be called with. Accepted for
            ///        interface stability with the spec but otherwise unused: the FIFO re-blocking
            ///        design needs no per-call scratch sized off it.
            StreamingConvolution(std::uint32_t partition_size, const float* impulse_response,
                                 std::uint32_t impulse_length, std::uint32_t max_block);
            /// @brief Convolves frames samples; output lags input by latency_samples().
            /// @param input frames samples. @param output frames samples. @param frames <= max_block.
            void process(const float* input, float* output, std::uint32_t frames);
            /// @brief Clears the FIFO and the convolver's history.
            void reset();
            /// @brief Delay from input to output.
            /// @return partition_size.
            std::uint32_t latency_samples() const
            {
                return partition_size_;
            }
            /// @brief Chooses the spec'd partition for a sample rate.
            /// @param sample_rate Base rate in Hz.
            /// @return 128 at or below 50 kHz, 256 above.
            static std::uint32_t partition_size_for(double sample_rate);

        private:
            std::uint32_t partition_size_;
            PartitionedConvolution convolution_;
            std::vector<float> input_fifo_;  // partition_size samples being gathered
            std::vector<float> output_fifo_; // partition_size samples being drained
            std::uint32_t fifo_fill_ =
                0; // samples in input_fifo_; output_fifo_ has partition_size - fifo_fill_ left
    };
} // namespace SushiDSP
