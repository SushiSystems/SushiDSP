/**************************************************************************/
/* modal_solver.hpp                                                       */
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
 * @file modal_solver.hpp
 * @brief The shoebox's eigenmodes below the Schroeder frequency, their damping and their gains.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <cstdint>
#include <vector>

#include <SushiDSP/nodes/room/image_source_solver.hpp>
#include <SushiDSP/nodes/room/modal_design.hpp>
#include <SushiDSP/nodes/room/room_scene.hpp>

namespace SushiDSP::room
{
    /// @brief Enumerates the shoebox's eigenmodes below the Schroeder frequency, damps each by the
    /// first-order perturbation over the six faces, and evaluates its mode shape at the cabinet
    /// and at the listener's receiver: the two ears or the microphone. Control thread. Spec
    /// §9a.2 and §9a.4.
    class ModalSolver
    {
        public:
            /// @brief Names the crossover below which the layer switches itself off, in hertz.
            static constexpr double K_MIN_CROSSOVER_HZ = 40.0;

            /// @brief Holds half the inter-ear spacing, in metres.
            static constexpr double K_EAR_HALF_SPACING_METRES = 0.085;

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

            /// @brief Holds what the bank takes from the room alone: the modes kept and a design
            /// whose resonators, count, crossover and level are final and whose gains are unset.
            struct Shell
            {
                    /// @brief Stores the modes the design keeps, sorted by frequency.
                    std::vector<Mode> modes;

                    /// @brief Stores the resonators; every source_gain and ear_gain is zero.
                    ModalDesign design;
            };

            /// @brief Returns the room's part of the bank. No pose enters it.
            /// @pre crossover_hz is under half of sample_rate.
            static Shell shell(const RoomScene& scene, double sample_rate, double crossover_hz);

            /// @brief Returns the whole bank from a room's part: every mode's shape at the cabinet
            /// and at the listener's receiver.
            /// @pre shell was built from this scene's room.
            static ModalDesign design(const Shell& shell, const RoomScene& scene,
                                      ListenerKind listener);

            /// @brief Returns the whole bank, its octave below the crossover scaled to DiffuseField.
            /// A crossover under K_MIN_CROSSOVER_HZ returns count = 0 and crossover_hz = 0; a count past
            /// K_MAX_MODES lowers crossover_hz to the first mode left out. Every section's numerator
            /// already carries level_scale over the mode's norm, so source_gain and ear_gain hold
            /// psi alone: at the two ears about listener_position for Ears, at microphone_position
            /// in both for Microphone.
            /// @pre scene.is_valid() holds.
            /// @pre crossover_hz is under half of sample_rate.
            static ModalDesign design(const RoomScene& scene, double sample_rate,
                                      double crossover_hz, ListenerKind listener);
    };
} // namespace SushiDSP::room
