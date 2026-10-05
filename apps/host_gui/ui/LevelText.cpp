/****************************************************************/
/* LevelText.cpp                                                */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

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
