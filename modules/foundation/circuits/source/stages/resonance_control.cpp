/**************************************************************************/
/* resonance_control.cpp                                                  */
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
 * @file resonance_control.cpp
 * @brief Implements the low shelf of the Resonance or Deep control.
 * @author Mustafa Garip
 */

#include "SushiDSP/circuits/stages/resonance_control.hpp"

#include <algorithm>

namespace SushiDSP
{
    namespace
    {
        constexpr double K_SHELF_Q = 0.7071067811865476;
    }

    ResonanceControl::ResonanceControl(double sample_rate, float corner_hz, float maximum_boost_db)
        : sample_rate_(sample_rate), corner_hz_(corner_hz), maximum_boost_db_(maximum_boost_db)
    {
        set_depth(0.0f);
    }

    void ResonanceControl::set_depth(float depth)
    {
        depth_ = std::clamp(depth, 0.0f, 1.0f);
        shelf_.set_low_shelf(sample_rate_, corner_hz_, boost_db(), K_SHELF_Q);
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
} // namespace SushiDSP
