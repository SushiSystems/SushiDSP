/****************************************************************/
/* SphericalHeadHrtf.hpp                                        */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

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
        /// with its diffuse-field average pair, each ear's power averaged over the sphere by
        /// solid angle and folded to minimum phase, and its third-order decoder, each grid
        /// pair delayed by its ITD and summed at its solid angle and max-rE encode weights.
        /// Spec §6.4 and §9.4.
        static HrtfSet build(double sample_rate);

        /// @brief Returns Woodworth's (a / c) * (theta + sin theta) in seconds, theta the lateral
        /// angle of the azimuth; positive means the right ear is later.
        static double interaural_time_difference(double azimuth_rad) noexcept;
    };
}
