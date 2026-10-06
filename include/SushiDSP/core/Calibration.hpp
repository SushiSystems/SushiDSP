/**************************************************************************/
/* Calibration.hpp                                                        */
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
 * @file Calibration.hpp
 * @brief Defines the full-scale calibration voltage and the decibel conversions.
 * @author Mustafa Garip
 */

#pragma once

#include <algorithm>
#include <cmath>

namespace SushiDSP
{
    /// @brief The peak voltage a full-scale (+/-1.0) sample represents at the amp's input, so a
    /// module's circuit model can be driven in volts rather than in normalized units.
    constexpr float kFullScaleVolts = 2.0f;

    /// @brief Converts a level in decibels to a linear amplitude multiplier.
    /// @param db The level in decibels; 0 dB is unity.
    /// @return The linear gain.
    inline float decibels_to_gain(float db)
    {
        return std::pow(10.0f, db / 20.0f);
    }

    /// @brief Converts a linear amplitude multiplier to decibels.
    /// @param gain The linear gain; values below 1e-9 are floored to keep the log finite.
    /// @return The level in decibels.
    inline float gain_to_decibels(float gain)
    {
        return 20.0f * std::log10(std::max(gain, 1e-9f));
    }
}
