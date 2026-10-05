/****************************************************************/
/* Dd3Converter.cpp                                             */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file Dd3Converter.cpp
 * @brief Implements the DD-3's 12-bit converter characteristic and ring.
 * @author Mustafa Garip & Sushi Systems
 */

#include "SushiDSP/dd3/Dd3Converter.hpp"

#include <algorithm>
#include <cassert>
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
        length_ = length_for_mode(mode_);
        ring_.assign(kMaximumRingLength, 0.0f);
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
        const long code_raw = std::lround(volts / kStepVolts);
        const long code = std::clamp<long>(code_raw, kMinimumCode, kMaximumCode);
        return static_cast<double>(code) * kStepVolts;
    }
}
