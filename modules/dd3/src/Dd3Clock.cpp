/**************************************************************************/
/* Dd3Clock.cpp                                                           */
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
