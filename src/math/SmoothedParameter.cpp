/**************************************************************************/
/* SmoothedParameter.cpp                                                  */
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

#include "SushiDSP/math/SmoothedParameter.hpp"

#include <cmath>

namespace SushiDSP
{
    SmoothedParameter::SmoothedParameter(float initial_value)
        : current_(initial_value), target_(initial_value)
    {
    }

    void SmoothedParameter::prepare(double sample_rate, float time_constant_ms)
    {
        const double samples = sample_rate * static_cast<double>(time_constant_ms) * 0.001;
        coefficient_ = samples > 0.0 ? static_cast<float>(std::exp(-1.0 / samples)) : 0.0f;
    }

    void SmoothedParameter::set_target(float target)
    {
        target_ = target;
    }

    float SmoothedParameter::next()
    {
        current_ = target_ + (current_ - target_) * coefficient_;
        return current_;
    }

    float SmoothedParameter::skip(std::uint32_t samples)
    {
        const float decay = std::pow(coefficient_, static_cast<float>(samples));
        current_ = target_ + (current_ - target_) * decay;
        return current_;
    }

    bool SmoothedParameter::is_settled(float tolerance) const
    {
        return std::fabs(current_ - target_) <= tolerance;
    }

    void SmoothedParameter::snap(float value)
    {
        current_ = value;
        target_ = value;
    }
}
