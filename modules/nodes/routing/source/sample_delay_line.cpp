/**************************************************************************/
/* sample_delay_line.cpp                                                  */
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
 * @file sample_delay_line.cpp
 * @brief Implements the whole-sample delay line.
 * @author Mustafa Garip
 */

#include "SushiDSP/routing/sample_delay_line.hpp"

#include <algorithm>

namespace SushiDSP
{
    void SampleDelayLine::prepare(std::size_t max_samples)
    {
        buffer_.assign(max_samples + 1, 0.0f);
        write_index_ = 0;
        delay_ = std::min(delay_, max_samples);
    }

    void SampleDelayLine::set_delay(std::size_t samples) noexcept
    {
        const std::size_t maximum = buffer_.empty() ? 0 : buffer_.size() - 1;
        delay_ = std::min(samples, maximum);
    }

    float SampleDelayLine::process(float sample) noexcept
    {
        if (buffer_.empty())
        {
            return sample;
        }

        buffer_[write_index_] = sample;
        const std::size_t size = buffer_.size();
        // delay_ < size, so the subtraction below cannot wrap past zero.
        const std::size_t read_index = (write_index_ + size - delay_) % size;
        write_index_ = (write_index_ + 1) % size;
        return buffer_[read_index];
    }

    void SampleDelayLine::reset() noexcept
    {
        std::fill(buffer_.begin(), buffer_.end(), 0.0f);
        write_index_ = 0;
    }

    std::size_t SampleDelayLine::max_delay() const noexcept
    {
        return buffer_.empty() ? 0 : buffer_.size() - 1;
    }
} // namespace SushiDSP
