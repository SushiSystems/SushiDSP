/****************************************************************/
/* Dd3Clock.cpp                                                 */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file Dd3Clock.cpp
 * @brief The DD-3's TIME-pot VCO law and its R56/C52 slew on log(fs).
 * @author Mustafa Garip & Sushi Systems
 */

#include "SushiDSP/dd3/Dd3Clock.hpp"

#include <algorithm>

namespace SushiDSP::dd3
{
    void Dd3Clock::prepare(double sample_rate)
    {
        control_.prepare(sample_rate, kSlewTimeConstantMs);
    }

    void Dd3Clock::set_time(float normalized)
    {
        control_.set_target(static_cast<float>(std::log(clock_for_time(normalized))));
    }

    double Dd3Clock::advance(std::uint32_t frames)
    {
        return std::exp(static_cast<double>(control_.skip(frames)));
    }

    double Dd3Clock::sample_rate_hz() const
    {
        return std::exp(static_cast<double>(control_.current()));
    }

    void Dd3Clock::reset()
    {
        control_.snap(control_.target());
    }

    double Dd3Clock::clock_for_time(double normalized)
    {
        const double clamped = std::clamp(normalized, 0.0, 1.0);
        return kFastRateHz * std::pow(kSlowRateHz / kFastRateHz, clamped);
    }
}
