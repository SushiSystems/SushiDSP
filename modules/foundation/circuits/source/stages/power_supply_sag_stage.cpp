/**************************************************************************/
/* power_supply_sag_stage.cpp                                             */
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
 * @file power_supply_sag_stage.cpp
 * @brief Implements the power-supply sag stage.
 * @author Mustafa Garip
 */

#include <SushiDSP/foundation/circuits/stages/power_supply_sag_stage.hpp>

#include <algorithm>
#include <cmath>

namespace SushiDSP
{
    PowerSupplySagStage::PowerSupplySagStage(double sample_rate, float sag_amount,
                                             float recovery_time_seconds)
        : envelope_coefficient_(
              1.0f - std::exp(-1.0f / (recovery_time_seconds * static_cast<float>(sample_rate)))),
          sag_amount_(sag_amount)
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
} // namespace SushiDSP
