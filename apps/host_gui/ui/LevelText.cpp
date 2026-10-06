/**************************************************************************/
/* LevelText.cpp                                                          */
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
 * @file LevelText.cpp
 * @brief Implements the conversions from a peak to decibels, a meter fill and a label.
 * @author Mustafa Garip
 */

#include "ui/LevelText.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

namespace ui
{
    float peak_to_decibels(float peak) noexcept
    {
        if (peak <= 0.0f)
        {
            return -std::numeric_limits<float>::infinity();
        }
        return 20.0f * std::log10(peak);
    }

    float decibels_to_fill(float decibels, float floor_db) noexcept
    {
        if (!std::isfinite(decibels))
        {
            return 0.0f;
        }
        if (floor_db >= 0.0f)
        {
            return decibels >= 0.0f ? 1.0f : 0.0f;
        }
        return std::clamp((decibels - floor_db) / -floor_db, 0.0f, 1.0f);
    }

    std::string decibel_label(float decibels)
    {
        if (!std::isfinite(decibels))
        {
            return "-inf dB";
        }
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "%.1f dB", static_cast<double>(decibels));
        return buffer;
    }
}
