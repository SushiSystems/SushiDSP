/****************************************************************/
/* AudioTaperPot.hpp                                            */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

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
