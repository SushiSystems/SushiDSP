/****************************************************************/
/* ResonanceControl.cpp                                         */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file ResonanceControl.cpp
 * @brief Implements the low shelf of the Resonance or Deep control.
 * @author Mustafa Garip
 */

#include "SushiDSP/circuits/stages/ResonanceControl.hpp"

#include <algorithm>

namespace SushiDSP
{
    namespace
    {
        constexpr double kShelfQ = 0.7071067811865476;
    }

    ResonanceControl::ResonanceControl(double sample_rate, float corner_hz, float maximum_boost_db)
        : sample_rate_(sample_rate), corner_hz_(corner_hz), maximum_boost_db_(maximum_boost_db)
    {
        set_depth(0.0f);
    }

    void ResonanceControl::set_depth(float depth)
    {
        depth_ = std::clamp(depth, 0.0f, 1.0f);
        shelf_.set_low_shelf(sample_rate_, corner_hz_, boost_db(), kShelfQ);
    }

    float ResonanceControl::process_sample(float input)
    {
        return shelf_.process_sample(input);
    }

    void ResonanceControl::reset()
    {
        shelf_.reset();
    }

    float ResonanceControl::boost_db() const
    {
        return depth_ * maximum_boost_db_;
    }
}
