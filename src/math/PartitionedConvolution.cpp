/****************************************************************/
/* PartitionedConvolution.cpp                                   */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

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
        const std::uint32_t partition_size = partition_size_;
        const std::uint32_t fft_size = fft_size_;
        const std::uint32_t partition_count = partition_count_;

        // Zero-pad the block to 2N. The copy overwrites the whole lower half, so only the upper
        // half needs clearing — and it is already zero every call after the first.
        float* const __restrict padded = zero_padded_scratch_.data();
        std::copy(input, input + partition_size, padded);
        std::fill(padded + partition_size, padded + fft_size, 0.0f);

        fft_.forward(padded, history_real_[write_index_].data(), history_imag_[write_index_].data());

        float* const __restrict accumulated_real = freq_real_accum_.data();
        float* const __restrict accumulated_imag = freq_imag_accum_.data();
        std::fill(accumulated_real, accumulated_real + fft_size, 0.0f);
        std::fill(accumulated_imag, accumulated_imag + fft_size, 0.0f);

        // Y = sum_k X[i-k] * H[k]: impulse-response partition k lines up with the input block
        // received k calls ago, which is the history slot k steps behind the one just written.
        for (std::uint32_t partition = 0; partition < partition_count; ++partition)
        {
            const std::uint32_t history_slot = (write_index_ + partition_count - partition) % partition_count;

            const float* const __restrict x_real = history_real_[history_slot].data();
            const float* const __restrict x_imag = history_imag_[history_slot].data();
            const float* const __restrict h_real = ir_real_[partition].data();
            const float* const __restrict h_imag = ir_imag_[partition].data();

            for (std::uint32_t bin = 0; bin < fft_size; ++bin)
            {
                accumulated_real[bin] += x_real[bin] * h_real[bin] - x_imag[bin] * h_imag[bin];
                accumulated_imag[bin] += x_real[bin] * h_imag[bin] + x_imag[bin] * h_real[bin];
            }
        }

        fft_.inverse(accumulated_real, accumulated_imag, time_domain_scratch_.data());

        // Overlap-add: the first half is this block's output once the tail carried over from the
        // previous call is added in; the second half becomes the tail for the next call.
        const float* const __restrict time_domain = time_domain_scratch_.data();
        float* const __restrict overlap = overlap_.data();
        float* const __restrict destination = output;
        for (std::uint32_t i = 0; i < partition_size; ++i)
        {
            destination[i] = time_domain[i] + overlap[i];
            overlap[i] = time_domain[partition_size + i];
        }

        write_index_ = (write_index_ + 1) % partition_count;
    }

    std::uint32_t PartitionedConvolution::partition_size() const
    {
        return partition_size_;
    }

    std::uint32_t PartitionedConvolution::partition_count() const
    {
        return partition_count_;
    }

    void PartitionedConvolution::reset()
    {
        for (auto& slot : history_real_)
        {
            std::fill(slot.begin(), slot.end(), 0.0f);
        }
        for (auto& slot : history_imag_)
        {
            std::fill(slot.begin(), slot.end(), 0.0f);
        }
        std::fill(overlap_.begin(), overlap_.end(), 0.0f);
        write_index_ = 0;
    }
}
