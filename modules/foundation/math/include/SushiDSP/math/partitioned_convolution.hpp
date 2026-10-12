/**************************************************************************/
/* partitioned_convolution.hpp                                            */
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
 * @file partitioned_convolution.hpp
 * @brief Declares uniform-partitioned FFT convolution against a fixed impulse response.
 * @author Mustafa Garip
 */

#pragma once

#include <cstdint>
#include <vector>

#include "SushiDSP/math/fft.hpp"

namespace SushiDSP
{
    /// @brief Uniform-partitioned overlap-add FFT convolution against a fixed impulse response.
    ///
    /// The impulse response is split into partition_count() blocks of partition_size samples,
    /// each transformed once in the constructor. Every process() call transforms one input block,
    /// stores it in a circular history of past input spectra, accumulates the partition products
    /// in the frequency domain, and overlap-adds the inverse transform against the tail saved by
    /// the previous call. Allocation-free after construction.
    class PartitionedConvolution
    {
        public:
            /// @brief Precomputes the FFT of every impulse-response partition.
            /// @param partition_size Block size in frames; also the FFT half-size (fft_size = 2*partition_size).
            ///        Must be a power of two so that the doubled transform size is one too.
            /// @param impulse_response The impulse response samples.
            /// @param impulse_length Number of samples in impulse_response.
            PartitionedConvolution(std::uint32_t partition_size, const float* impulse_response,
                                   std::uint32_t impulse_length);

            /// @brief Processes one block, convolving it against the impulse response.
            /// @param input partition_size input samples.
            /// @param output partition_size output samples (overlap-added).
            /// @note input and output must not alias each other.
            void process(const float* input, float* output);

            /// @brief The block size this instance was constructed for.
            /// @return The partition size in frames.
            std::uint32_t partition_size() const;

            /// @brief The number of impulse-response partitions.
            /// @return The partition count.
            std::uint32_t partition_count() const;

            /// @brief Clears the spectrum history and overlap tail, as if freshly constructed.
            /// The impulse-response partitions themselves are untouched.
            void reset();

        private:
            std::uint32_t partition_size_;
            std::uint32_t fft_size_;
            std::uint32_t partition_count_;
            Fft fft_;

            std::vector<std::vector<float>> ir_real_;
            std::vector<std::vector<float>> ir_imag_;
            std::vector<std::vector<float>> history_real_;
            std::vector<std::vector<float>> history_imag_;
            std::uint32_t write_index_ = 0;

            std::vector<float> overlap_;
            std::vector<float> zero_padded_scratch_;
            std::vector<float> freq_real_accum_;
            std::vector<float> freq_imag_accum_;
            std::vector<float> time_domain_scratch_;
    };
} // namespace SushiDSP
