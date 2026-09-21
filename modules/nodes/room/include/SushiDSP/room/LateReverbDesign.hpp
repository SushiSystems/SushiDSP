/**************************************************************************/
/* LateReverbDesign.hpp                                                   */
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
 * @file LateReverbDesign.hpp
 * @brief The control-thread design that turns a room into the late tail's FDN coefficients.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <array>
#include <cstdint>

#include "SushiDSP/room/EarlyTapSet.hpp"
#include "SushiDSP/room/FdnDesign.hpp"
#include "SushiDSP/room/ImageSourceSolver.hpp"
#include "SushiDSP/room/RoomScene.hpp"

namespace SushiDSP::room
{
    /// @brief Turns a room into sixteen line lengths, sixteen absorptive filters, an input gain
    /// and an output cascade that hand the tail what the early set leaves of DiffuseField in
    /// each band, and the two coherence filters. Control thread. Spec §6.2, §6.3 and §6.4.
    class LateReverbDesign
    {
    public:
        /// @brief Returns the whole FDN design for one scene, its early set and one listener.
        /// @pre sample_rate exceeds 2 * kBandCentresHz[kBandCount - 1]; the early taps are
        /// sorted by arrival, so tap 0 is the direct sound whenever count > 0.
        static FdnDesign design(const RoomScene& scene, const EarlyTapSet& early,
                                double sample_rate, ListenerKind listener);

        /// @brief Returns sixteen distinct primes spread logarithmically over
        /// [shortest_seconds, shortest_seconds * spread], in frames and in rising order.
        /// @pre spread is at least one.
        static std::array<std::uint32_t, FdnDesign::kLineCount>
            line_frames(double shortest_seconds, double spread, double sample_rate);
    };
}
