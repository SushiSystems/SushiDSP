/**************************************************************************/
/* StreamingConvolution.cpp                                               */
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
 * @file StreamingConvolution.cpp
 * @brief Implements the re-blocking front of PartitionedConvolution.
 * @author Mustafa Garip
 */

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
        // output_fifo_[fifo_fill_ ..) is the unemitted result of the previous partition (src/README.md).
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
