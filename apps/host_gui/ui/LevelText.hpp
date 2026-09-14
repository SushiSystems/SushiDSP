/**************************************************************************/
/* LevelText.hpp                                                          */
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

#pragma once

#include <string>

namespace ui
{
    /// @brief Converts a linear peak amplitude to decibels.
    /// @param peak The linear peak amplitude; a unity peak reads 0 dB.
    /// @return The level in decibels; negative infinity for peak <= 0.
    float peak_to_decibels(float peak) noexcept;

    /// @brief Maps a decibel level to a [0, 1] meter fill fraction.
    /// @param decibels The level to place on the meter.
    /// @param floor_db The level that fills to 0; decibels at or above 0 dB fill to 1.
    /// @return The fill fraction, clamped to [0, 1].
    float decibels_to_fill(float decibels, float floor_db = -60.0f) noexcept;

    /// @brief Formats a decibel level for the meter's numeric readout.
    /// @param decibels The level to format.
    /// @return "-inf dB" for negative infinity, otherwise one decimal place plus " dB".
    std::string decibel_label(float decibels);
}
