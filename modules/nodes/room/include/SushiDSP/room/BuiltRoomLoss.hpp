/****************************************************************/
/* BuiltRoomLoss.hpp                                            */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file BuiltRoomLoss.hpp
 * @brief The absorption a built room adds over its tabulated surfaces, per unit of surface.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <array>

#include "SushiDSP/room/BandResponse.hpp"

namespace SushiDSP::room
{
    /// @brief Holds the absorption coefficient a built room adds to every surface, per octave band
    /// 125 Hz to 4 kHz, fitted to the Motus room and the BRAS CR2 seminar room.
    /// Design: docs/design/ROOM_FURNISHINGS.md section 11.
    inline constexpr std::array<double, kBandCount> kBuiltRoomLoss{ 0.020, 0.025, 0.020,
                                                                    0.020, 0.020, 0.015 };
}
