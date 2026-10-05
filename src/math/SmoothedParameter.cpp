/****************************************************************/
/* SmoothedParameter.cpp                                        */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

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
