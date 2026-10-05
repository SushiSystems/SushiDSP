/****************************************************************/
/* PowerSupplySagStage.cpp                                      */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

#include "SushiDSP/circuits/stages/PowerSupplySagStage.hpp"

#include <algorithm>
#include <cmath>

namespace SushiDSP
{
    PowerSupplySagStage::PowerSupplySagStage(double sample_rate, float sag_amount, float recovery_time_seconds)
        : envelope_coefficient_(1.0f - std::exp(-1.0f / (recovery_time_seconds * static_cast<float>(sample_rate))))
        , sag_amount_(sag_amount)
    {
    }

    float PowerSupplySagStage::process_sample(float input)
    {
        const float rectified = std::abs(input);
        envelope_ += (rectified - envelope_) * envelope_coefficient_;

        const float gain_reduction = std::max(0.2f, 1.0f - sag_amount_ * envelope_);
        return input * gain_reduction;
    }

    void PowerSupplySagStage::reset()
    {
        envelope_ = 0.0f;
    }
}
