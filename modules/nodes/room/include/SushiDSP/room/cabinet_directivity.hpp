/**************************************************************************/
/* cabinet_directivity.hpp                                                */
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
 * @file cabinet_directivity.hpp
 * @brief The per-band directivity of a guitar cabinet modelled as pistons in a baffle.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <array>
#include <cstddef>
#include <vector>

#include "SushiDSP/room/band_response.hpp"
#include "SushiDSP/room/cabinet_geometry.hpp"

namespace SushiDSP::room
{
    /// @brief Evaluates a cabinet's rigid pistons in an infinite baffle in front of a closed box,
    /// as an energy mean over each octave band, on the control thread. Spec §7.
    class CabinetDirectivity
    {
        public:
            /// @brief Builds the model for one geometry.
            explicit CabinetDirectivity(const CabinetGeometry& geometry);

            /// @brief Returns the gain per band relative to on axis for a direction in the cabinet's
            /// own frame, each band the energy mean of the closed form over its octave.
            /// @param azimuth_rad 0 straight ahead, positive to the cabinet's left.
            /// @param elevation_rad 0 on the baffle's axis, positive up.
            BandResponse gain_for(double azimuth_rad, double elevation_rad) const noexcept;

            /// @brief Returns the directivity index per band, in decibels, integrated over a
            /// 2562-point spherical grid.
            std::array<float, K_BAND_COUNT> directivity_index_db() const;

            /// @brief Returns 2*J1(x)/x, the rigid-piston factor, with the x -> 0 limit of 1.
            static double piston_factor(double x) noexcept;

            /// @brief Returns the effective cone radius at one frequency, in metres: the cone's
            /// own radius below the breakup frequency, shrinking above it.
            double effective_radius(double frequency_hz) const noexcept;

            /// @brief Returns the pressure gain behind the baffle plane at each band centre,
            /// relative to in front of it.
            BandResponse rear_response() const noexcept;

        private:
            /// @brief Holds one unit direction of the spherical grid.
            struct Direction
            {
                    /// @brief Stores the component along the baffle's axis.
                    double forward;

                    /// @brief Stores the component to the cabinet's left.
                    double left;

                    /// @brief Stores the component upward.
                    double up;
            };

            /// @brief Returns the gain per band for a unit direction.
            BandResponse gain_for(const Direction& direction) const noexcept;

            /// @brief Returns the closed form at one frequency for a unit direction whose sine of
            /// the off-axis angle is given.
            double factor_at(double frequency_hz, const Direction& direction,
                             double sine_theta) const noexcept;

            /// @brief Returns the rear shelf's pressure gain at one frequency.
            double rear_shelf(double frequency_hz) const noexcept;

            /// @brief Builds the icosahedron subdivided four times, 2562 unit directions.
            static std::vector<Direction> build_grid();

            /// @brief Stores the geometry the model evaluates.
            CabinetGeometry geometry_;
    };
} // namespace SushiDSP::room
