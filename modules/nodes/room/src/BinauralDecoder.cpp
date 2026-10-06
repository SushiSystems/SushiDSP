/**************************************************************************/
/* BinauralDecoder.cpp                                                    */
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
 * @file BinauralDecoder.cpp
 * @brief Re-blocks sixteen channels onto one partition and sums 32 convolutions into two ears.
 * @author Mustafa Garip & Sushi Systems
 */

#include "SushiDSP/room/BinauralDecoder.hpp"

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <utility>

#include "SushiDSP/math/StreamingConvolution.hpp"

namespace SushiDSP::room
{
    namespace
    {
        /// @brief Adds count values of source onto destination.
        void accumulate(const float* __restrict source, float* __restrict destination,
                        std::uint32_t count) noexcept
        {
            for (std::uint32_t i = 0; i < count; ++i)
            {
                destination[i] += source[i];
            }
        }
    }

    BinauralDecoder::BinauralDecoder()
        : partition_size_(0)
        , fill_(0)
    {
    }

    void BinauralDecoder::prepare(const HrtfSet& set, double sample_rate,
                                  std::uint32_t max_block_frames)
    {
        static_cast<void>(max_block_frames);
        const std::vector<float>& filters = set.decoder_filters();
        const std::size_t expected =
            static_cast<std::size_t>(kEars) * kChannels * HrtfSet::kDecoderTaps;
        if (set.decoder_order() != AmbisonicEncoder::kOrder || filters.size() != expected)
        {
            throw std::runtime_error("BinauralDecoder: the set carries no third-order decoder");
        }

        const std::uint32_t partition_size = StreamingConvolution::partition_size_for(sample_rate);
        std::vector<PartitionedConvolution> convolvers;
        convolvers.reserve(static_cast<std::size_t>(kEars) * kChannels);
        for (std::uint32_t filter = 0; filter < kEars * kChannels; ++filter)
        {
            convolvers.emplace_back(
                partition_size,
                filters.data() + static_cast<std::size_t>(filter) * HrtfSet::kDecoderTaps,
                HrtfSet::kDecoderTaps);
        }

        // Every buffer is sized here and only here: process() allocates nothing.
        convolvers_ = std::move(convolvers);
        input_partition_.assign(static_cast<std::size_t>(kChannels) * partition_size, 0.0f);
        output_partition_.assign(static_cast<std::size_t>(kEars) * partition_size, 0.0f);
        scratch_.assign(partition_size, 0.0f);
        partition_size_ = partition_size;
        fill_ = 0;
    }

    bool BinauralDecoder::ready() const noexcept
    {
        return partition_size_ != 0;
    }

    std::uint32_t BinauralDecoder::latency_frames() const noexcept
    {
        return partition_size_;
    }

    void BinauralDecoder::process(const float* const* inputs, float* left, float* right,
                                  std::uint32_t frames) noexcept
    {
        const std::uint32_t partition_size = partition_size_;
        float* const __restrict input_partition = input_partition_.data();
        const float* const __restrict output_partition = output_partition_.data();
        std::uint32_t done = 0;
        while (done < frames)
        {
            const std::uint32_t count = std::min(frames - done, partition_size - fill_);

            // One cursor reads output and writes input; see modules/nodes/room/README.md.
            std::copy_n(output_partition + fill_, count, left + done);
            std::copy_n(output_partition + partition_size + fill_, count, right + done);
            for (std::uint32_t channel = 0; channel < kChannels; ++channel)
            {
                std::copy_n(inputs[channel] + done, count,
                            input_partition +
                                static_cast<std::size_t>(channel) * partition_size + fill_);
            }

            fill_ += count;
            done += count;
            if (fill_ == partition_size)
            {
                decode_partition();
                fill_ = 0;
            }
        }
    }

    void BinauralDecoder::decode_partition() noexcept
    {
        const std::uint32_t partition_size = partition_size_;
        const float* const input_partition = input_partition_.data();
        float* const output_partition = output_partition_.data();
        float* const scratch = scratch_.data();

        // Each of the sixteen channels is transformed twice; see modules/nodes/room/README.md.
        for (std::uint32_t ear = 0; ear < kEars; ++ear)
        {
            float* const ear_output =
                output_partition + static_cast<std::size_t>(ear) * partition_size;
            std::fill_n(ear_output, partition_size, 0.0f);
            for (std::uint32_t channel = 0; channel < kChannels; ++channel)
            {
                convolvers_[static_cast<std::size_t>(ear) * kChannels + channel].process(
                    input_partition + static_cast<std::size_t>(channel) * partition_size,
                    scratch);
                accumulate(scratch, ear_output, partition_size);
            }
        }
    }

    void BinauralDecoder::reset() noexcept
    {
        for (PartitionedConvolution& convolver : convolvers_)
        {
            convolver.reset();
        }
        std::fill(input_partition_.begin(), input_partition_.end(), 0.0f);
        std::fill(output_partition_.begin(), output_partition_.end(), 0.0f);
        fill_ = 0;
    }
}
