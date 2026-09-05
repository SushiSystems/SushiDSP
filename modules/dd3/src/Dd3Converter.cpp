/**************************************************************************/
/* Dd3Converter.cpp                                                       */
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

/**
 * @file Dd3Converter.cpp
 * @brief Implements the DD-3's 12-bit converter characteristic and ring.
 * @author Mustafa Garip & Sushi Systems
 */

#include "SushiDSP/dd3/Dd3Converter.hpp"

#include <algorithm>
#include <cmath>

namespace SushiDSP::dd3
{
    std::uint32_t Dd3Converter::length_for_mode(Dd3Mode mode)
    {
        switch (mode)
        {
            case Dd3Mode::Short:  return kShortLength;
            case Dd3Mode::Middle: return kMiddleLength;
            case Dd3Mode::Long:   return kLongLength;
            case Dd3Mode::Hold:   return kLongLength;
        }
        return kLongLength;
    }

    void Dd3Converter::prepare()
    {
        ring_.assign(kMaximumRingLength, 0.0f);
        index_ = 0u;
    }

    void Dd3Converter::set_mode(Dd3Mode mode)
    {
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
        const long code_raw = std::lround(volts / kStepVolts);
        const long code = std::clamp<long>(code_raw, kMinimumCode, kMaximumCode);
        return static_cast<double>(code) * kStepVolts;
    }
}
