/**************************************************************************/
/* dd3_converter.cpp                                                      */
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
 * @file dd3_converter.cpp
 * @brief Implements the DD-3's 12-bit converter characteristic and ring.
 * @author Mustafa Garip & Sushi Systems
 */

#include <SushiDSP/nodes/dd3/dd3_converter.hpp>

#include <algorithm>
#include <cassert>
#include <cmath>

namespace SushiDSP::dd3
{
    std::uint32_t Dd3Converter::length_for_mode(Dd3Mode mode)
    {
        switch (mode)
        {
        case Dd3Mode::Short:
            return K_SHORT_LENGTH;
        case Dd3Mode::Middle:
            return K_MIDDLE_LENGTH;
        case Dd3Mode::Long:
            return K_LONG_LENGTH;
        case Dd3Mode::Hold:
            return K_LONG_LENGTH;
        }
        return K_LONG_LENGTH;
    }

    void Dd3Converter::prepare()
    {
        length_ = length_for_mode(mode_);
        ring_.assign(K_MAXIMUM_RING_LENGTH, 0.0f);
        index_ = 0u;
    }

    void Dd3Converter::set_mode(Dd3Mode mode)
    {
        // Before prepare() sizes ring_, only mode_ is recorded; see modules/nodes/dd3/README.md.
        if (ring_.empty())
        {
            mode_ = mode;
            return;
        }

        const std::uint32_t new_length = length_for_mode(mode);
        if (new_length != length_)
        {
            std::fill(ring_.begin(), ring_.end(), 0.0f);
            index_ = 0u;
        }
        length_ = new_length;
        mode_ = mode;
    }

    float Dd3Converter::process_sample(float volts)
    {
        assert(!ring_.empty() && "Dd3Converter::process_sample requires prepare() first");
        if (ring_.empty())
        {
            return 0.0f;
        }

        const float out = ring_[index_];
        if (mode_ != Dd3Mode::Hold)
        {
            ring_[index_] = static_cast<float>(quantise(static_cast<double>(volts)));
        }
        index_ = (index_ + 1u) % length_;
        return out;
    }

    void Dd3Converter::reset()
    {
        std::fill(ring_.begin(), ring_.end(), 0.0f);
        index_ = 0u;
    }

    std::uint32_t Dd3Converter::ring_length() const
    {
        return length_;
    }

    double Dd3Converter::quantise(double volts)
    {
        const long code_raw = std::lround(volts / K_STEP_VOLTS);
        const long code = std::clamp<long>(code_raw, K_MINIMUM_CODE, K_MAXIMUM_CODE);
        return static_cast<double>(code) * K_STEP_VOLTS;
    }
} // namespace SushiDSP::dd3
