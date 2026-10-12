/**************************************************************************/
/* audio_taper_pot.hpp                                                    */
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
 * @file audio_taper_pot.hpp
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
} // namespace SushiDSP
