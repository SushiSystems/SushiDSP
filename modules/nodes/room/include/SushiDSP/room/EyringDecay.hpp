/**************************************************************************/
/* EyringDecay.hpp                                                        */
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
 * @file EyringDecay.hpp
 * @brief The room's decay time per band and the Schroeder frequency that follows from it.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <array>

#include "SushiDSP/room/BandResponse.hpp"
#include "SushiDSP/room/RoomScene.hpp"

namespace SushiDSP::room
{
    /// @brief Evaluates Eyring's decay time per band and the Schroeder frequency of a scene, on
    /// the control thread, with the air and the furnishings both in the volumetric term. Eyring
    /// rather than Sabine: spec §6.3.
    class EyringDecay
    {
    public:
        /// @brief Returns 0.161 * V / (-S * ln(1 - mean_alpha(f)) + 4 * m(f) * V) per band, in
        /// seconds, where m(f) = m_air(f) + A_f(f) / 4V, so the furnishings' area A_f joins the
        /// denominator; a band whose mean absorption reaches 1 returns 0 s.
        /// @pre scene.is_valid() holds.
        static std::array<double, kBandCount> rt60_seconds(const RoomScene& scene);

        /// @brief Returns 2000 * sqrt(RT60(500 Hz) / V), the frequency below which the room is
        /// modal, in hertz.
        /// @pre scene.is_valid() holds.
        static double schroeder_frequency_hz(const RoomScene& scene);
    };
}
