/**************************************************************************/
/* AudioTaperPot.hpp                                                      */
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
 * @file AudioTaperPot.hpp
 * @brief The audio taper the corpus gives a volume pot.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <cmath>

namespace SushiDSP
{
    /// @brief Returns the voltage division an audio-taper volume pot makes at one position,
    /// position to the three halves.
    /// @param normalized Pot position in [0, 1].
    /// @return The fraction of its input the wiper passes, 0 at the bottom and 1 wide open.
    inline float audio_taper_pot(float normalized)
    {
        return normalized * std::sqrt(normalized);
    }
}
