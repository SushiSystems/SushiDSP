/**************************************************************************/
/* ModalSolver.hpp                                                        */
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
 * @file ModalSolver.hpp
 * @brief The shoebox's eigenmodes below the Schroeder frequency, their damping and their gains.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <cstdint>
#include <vector>

#include "SushiDSP/room/ModalDesign.hpp"
#include "SushiDSP/room/RoomScene.hpp"

namespace SushiDSP::room
{
    /// @brief Enumerates the shoebox's eigenmodes below the Schroeder frequency, damps each by the
    /// first-order perturbation over the six faces, and evaluates its mode shape at the cabinet
    /// and the two ears. Control thread. Spec §9a.2 and §9a.4.
    class ModalSolver
    {
    public:
        /// @brief Names the crossover below which the layer switches itself off, in hertz.
        static constexpr double kMinCrossoverHz = 40.0;

        /// @brief Holds half the inter-ear spacing, in metres.
        static constexpr double kEarHalfSpacingMetres = 0.085;

        /// @brief Holds one eigenmode of the rigid shoebox.
        struct Mode
        {
            /// @brief Stores the mode index along x.
            std::uint32_t nx;

            /// @brief Stores the mode index along y.
            std::uint32_t ny;

            /// @brief Stores the mode index along z.
            std::uint32_t nz;

            /// @brief Stores (c / 2) sqrt((nx / Lx)^2 + (ny / Ly)^2 + (nz / Lz)^2), in hertz.
            double frequency_hz;

            /// @brief Stores the 60 dB decay from the first-order perturbation damping, in seconds.
            double decay_seconds;
        };

        /// @brief Returns every mode strictly under max_frequency_hz, the (0, 0, 0) pressure mode
        /// left out, sorted by frequency.
        static std::vector<Mode> enumerate(const RoomScene& scene, double max_frequency_hz);

        /// @brief Returns Weyl's count (4 pi / 3) V (f / c)^3 + (pi / 4) S (f / c)^2 + (L / 8)
        /// (f / c) with L = 4 (Lx + Ly + Lz).
        static double estimated_count(const RoomScene& scene, double frequency_hz);

        /// @brief Returns cos(nx pi x / Lx) cos(ny pi y / Ly) cos(nz pi z / Lz) at position.
        static double mode_shape(const Mode& mode, const RoomScene& scene,
                                 const Vec3& position) noexcept;

        /// @brief Returns the whole bank. A crossover under kMinCrossoverHz returns count = 0 and
        /// crossover_hz = 0; a count past kMaxModes lowers crossover_hz to the first mode left
        /// out. Every section's numerator already carries level_scale over the mode's norm, so
        /// source_gain and ear_gain hold psi alone and the bank applies nothing further.
        /// @pre crossover_hz is under half of sample_rate.
        static ModalDesign design(const RoomScene& scene, double sample_rate, double crossover_hz);
    };
}
