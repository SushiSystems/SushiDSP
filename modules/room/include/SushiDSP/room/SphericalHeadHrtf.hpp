/**************************************************************************/
/* SphericalHeadHrtf.hpp                                                  */
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
 * @file SphericalHeadHrtf.hpp
 * @brief An HrtfSet built from the rigid-sphere model, for a room with no measured set on disk.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include "SushiDSP/room/HrtfSet.hpp"

namespace SushiDSP::room
{
    /// @brief Builds an HrtfSet from the Duda-Martens rigid-sphere model with Woodworth's ITD,
    /// on the control thread. Correct ITD and ILD, no pinna cues: front and back sound alike.
    /// Spec §9.2.
    class SphericalHeadHrtf
    {
    public:
        /// @brief Holds the sphere radius in metres.
        static constexpr double kHeadRadiusMetres = 0.0875;

        /// @brief Holds the speed of sound in metres per second.
        static constexpr double kSpeedOfSound = 343.0;

        /// @brief Counts the azimuth steps of the grid: every 10 degrees around the head.
        static constexpr std::uint32_t kAzimuthSteps = 36;

        /// @brief Counts the elevation rings between the poles: -80 to 80 degrees in steps of 10.
        static constexpr std::uint32_t kElevationRings = 17;

        /// @brief Counts the grid directions: the rings, plus the two poles.
        static constexpr std::uint32_t kDirectionCount = kAzimuthSteps * kElevationRings + 2;

        /// @brief Builds a set on a 10-degree azimuth by 10-degree elevation grid, 614 directions,
        /// with its diffuse-field average pair: each ear's power averaged over the sphere by
        /// solid angle and folded to minimum phase. Spec §6.4.
        static HrtfSet build(double sample_rate);

        /// @brief Returns Woodworth's (a / c) * (theta + sin theta) in seconds, theta the lateral
        /// angle of the azimuth; positive means the right ear is later.
        static double interaural_time_difference(double azimuth_rad) noexcept;
    };
}
