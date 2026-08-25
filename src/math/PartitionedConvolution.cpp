/**************************************************************************/
/* PartitionedConvolution.cpp                                             */
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

#include <algorithm>
#include <cassert>

#include "SushiDSP/math/PartitionedConvolution.hpp"

namespace SushiDSP
{
    namespace
    {
        /// @brief ceil(impulse_length / partition_size), floored at one partition.
        ///
        /// A zero-length impulse response still gets one (all-zero) partition so that the
        /// circular history has at least one slot and the modulo in process() is well defined.
        std::uint32_t partition_count_for(std::uint32_t partition_size, std::uint32_t impulse_length)
        {
            if (partition_size == 0 || impulse_length == 0)
            {
                return 1;
            }
            return (impulse_length + partition_size - 1) / partition_size;
        }
    }

    PartitionedConvolution::PartitionedConvolution(
        std::uint32_t partition_size, const float* impulse_response, std::uint32_t impulse_length)
        : partition_size_(partition_size)
        , fft_size_(partition_size * 2u)
        , partition_count_(partition_count_for(partition_size, impulse_length))
        , fft_(fft_size_)
        , ir_real_(partition_count_, std::vector<float>(fft_size_, 0.0f))
        , ir_imag_(partition_count_, std::vector<float>(fft_size_, 0.0f))
        , history_real_(partition_count_, std::vector<float>(fft_size_, 0.0f))
        , history_imag_(partition_count_, std::vector<float>(fft_size_, 0.0f))
        , overlap_(partition_size_, 0.0f)
        , zero_padded_scratch_(fft_size_, 0.0f)
        , freq_real_accum_(fft_size_, 0.0f)
        , freq_imag_accum_(fft_size_, 0.0f)
        , time_domain_scratch_(fft_size_, 0.0f)
    {
        assert(partition_size_ > 0 && "partition_size must be non-zero");

        // Each partition is the impulse-response slice [k*N, (k+1)*N) zero-padded to 2N. The
        // second half must stay zero: it is the headroom that turns the FFT's circular
        // convolution into the linear one the overlap-add reconstruction assumes.
        for (std::uint32_t partition = 0; partition < partition_count_; ++partition)
        {
            std::fill(zero_padded_scratch_.begin(), zero_padded_scratch_.end(), 0.0f);
            const std::uint32_t start = partition * partition_size_;
            for (std::uint32_t i = 0; i < partition_size_; ++i)
            {
                const std::uint32_t source_index = start + i;
                if (source_index < impulse_length)
                {
                    zero_padded_scratch_[i] = impulse_response[source_index];
                }
            }
            fft_.forward(zero_padded_scratch_.data(), ir_real_[partition].data(), ir_imag_[partition].data());
        }

        std::fill(zero_padded_scratch_.begin(), zero_padded_scratch_.end(), 0.0f);
    }

    void PartitionedConvolution::process(const float* input, float* output)
    {
        // Zero-pad the block to 2N. The copy overwrites the whole lower half, so only the upper
        // half needs clearing — and it is already zero every call after the first.
        std::copy(input, input + partition_size_, zero_padded_scratch_.begin());
        std::fill(zero_padded_scratch_.begin() + partition_size_, zero_padded_scratch_.end(), 0.0f);

        fft_.forward(zero_padded_scratch_.data(), history_real_[write_index_].data(), history_imag_[write_index_].data());

        std::fill(freq_real_accum_.begin(), freq_real_accum_.end(), 0.0f);
        std::fill(freq_imag_accum_.begin(), freq_imag_accum_.end(), 0.0f);

        // Y = sum_k X[i-k] * H[k]: impulse-response partition k lines up with the input block
        // received k calls ago, which is the history slot k steps behind the one just written.
        for (std::uint32_t partition = 0; partition < partition_count_; ++partition)
        {
            const std::uint32_t history_slot = (write_index_ + partition_count_ - partition) % partition_count_;

            const std::vector<float>& x_real = history_real_[history_slot];
            const std::vector<float>& x_imag = history_imag_[history_slot];
            const std::vector<float>& h_real = ir_real_[partition];
            const std::vector<float>& h_imag = ir_imag_[partition];

            for (std::uint32_t bin = 0; bin < fft_size_; ++bin)
            {
                freq_real_accum_[bin] += x_real[bin] * h_real[bin] - x_imag[bin] * h_imag[bin];
                freq_imag_accum_[bin] += x_real[bin] * h_imag[bin] + x_imag[bin] * h_real[bin];
            }
        }

        fft_.inverse(freq_real_accum_.data(), freq_imag_accum_.data(), time_domain_scratch_.data());

        // Overlap-add: the first half is this block's output once the tail carried over from the
        // previous call is added in; the second half becomes the tail for the next call.
        for (std::uint32_t i = 0; i < partition_size_; ++i)
        {
            output[i]  = time_domain_scratch_[i] + overlap_[i];
            overlap_[i] = time_domain_scratch_[partition_size_ + i];
        }

        write_index_ = (write_index_ + 1) % partition_count_;
    }

    std::uint32_t PartitionedConvolution::partition_size() const
    {
        return partition_size_;
    }

    std::uint32_t PartitionedConvolution::partition_count() const
    {
        return partition_count_;
    }
}
