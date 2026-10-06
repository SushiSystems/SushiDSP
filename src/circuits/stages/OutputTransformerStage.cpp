/**************************************************************************/
/* OutputTransformerStage.cpp                                             */
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
 * @file OutputTransformerStage.cpp
 * @brief Implements the output transformer stage.
 * @author Mustafa Garip
 */

// _USE_MATH_DEFINES must precede every include: it is what makes MSVC's <cmath> declare M_PI.
#define _USE_MATH_DEFINES

#include "SushiDSP/circuits/stages/OutputTransformerStage.hpp"

#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{
    /// @brief The secondary's mean-tracking corner as a fraction of the rolloff corner
    /// (docs/archive/superpowers/specs/2026-09-07-grid-knee-antialiasing-design.md section 14).
    constexpr double kSecondaryMeanCornerFraction = 0.1;
}

namespace SushiDSP
{
    OutputTransformerStage::OutputTransformerStage(
        double sample_rate, float low_rolloff_hz, float leakage_shelf_hz, float saturation_drive)
        : saturation_drive_(saturation_drive)
        , secondary_mean_coefficient_(
              1.0 - std::exp(-2.0 * M_PI * kSecondaryMeanCornerFraction * low_rolloff_hz / sample_rate))
    {
        rolloff_.set_high_pass(sample_rate, low_rolloff_hz, 0.707);
        leakage_.set_low_pass(sample_rate, leakage_shelf_hz, 0.707);
    }

    float OutputTransformerStage::process_sample(float input)
    {
        float x = rolloff_.process_sample(input);
        x = leakage_.process_sample(x);
        const double saturated = std::tanh(x * saturation_drive_) / saturation_drive_;
        // The mean is tracked in double so what is subtracted is the mean and not its rounding.
        secondary_mean_ += (saturated - secondary_mean_) * secondary_mean_coefficient_;
        return static_cast<float>(saturated - secondary_mean_);
    }

    void OutputTransformerStage::reset()
    {
        rolloff_.reset();
        leakage_.reset();
        secondary_mean_ = 0.0;
    }
}
