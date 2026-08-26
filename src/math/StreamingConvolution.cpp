/**************************************************************************/
/* StreamingConvolution.cpp                                               */
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

#include "SushiDSP/math/StreamingConvolution.hpp"
#include <algorithm>
namespace SushiDSP
{
    StreamingConvolution::StreamingConvolution(std::uint32_t partition_size, const float* impulse_response,
                                               std::uint32_t impulse_length, std::uint32_t)
        : partition_size_(partition_size)
        , convolution_(partition_size, impulse_response, impulse_length)
        , input_fifo_(partition_size, 0.0f)
        , output_fifo_(partition_size, 0.0f)
    {
    }

    void StreamingConvolution::process(const float* input, float* output, std::uint32_t frames)
    {
        // Invariant: output_fifo_[fifo_fill_ ..) is the not-yet-emitted result of the previous
        // partition, so reading one output sample and writing one input sample keep the same
        // cursor. Starting from a zero output_fifo_ is what makes the latency exactly one partition.
        for (std::uint32_t i = 0; i < frames; ++i)
        {
            output[i] = output_fifo_[fifo_fill_];
            input_fifo_[fifo_fill_] = input[i];
            ++fifo_fill_;
            if (fifo_fill_ == partition_size_)
            {
                convolution_.process(input_fifo_.data(), output_fifo_.data());
                fifo_fill_ = 0;
            }
        }
    }

    void StreamingConvolution::reset()
    {
        convolution_.reset();
        std::fill(input_fifo_.begin(), input_fifo_.end(), 0.0f);
        std::fill(output_fifo_.begin(), output_fifo_.end(), 0.0f);
        fifo_fill_ = 0;
    }

    std::uint32_t StreamingConvolution::partition_size_for(double sample_rate)
    {
        return sample_rate > 50000.0 ? 256u : 128u;
    }
}
