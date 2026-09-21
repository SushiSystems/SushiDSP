/**************************************************************************/
/* EyringDecay.hpp                                                        */
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
    /// the control thread. Eyring rather than Sabine: spec §6.3.
    class EyringDecay
    {
    public:
        /// @brief Returns 0.161 * V / (-S * ln(1 - mean_alpha(f)) + 4 * m(f) * V) per band, in
        /// seconds; a band whose mean absorption reaches 1 returns 0 s.
        static std::array<double, kBandCount> rt60_seconds(const RoomScene& scene);

        /// @brief Returns 2000 * sqrt(RT60(500 Hz) / V), the frequency below which the room is
        /// modal, in hertz.
        static double schroeder_frequency_hz(const RoomScene& scene);
    };
}
