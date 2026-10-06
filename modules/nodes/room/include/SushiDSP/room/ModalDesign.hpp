/**************************************************************************/
/* ModalDesign.hpp                                                        */
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
 * @file ModalDesign.hpp
 * @brief The modal bank's resonators and gains as the control thread publishes them.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace SushiDSP::room
{
    /// @brief Holds the wave layer's resonators, one per room mode below f_s. Spec §9a.2.
    struct ModalDesign
    {
        /// @brief Bounds the modes one design carries.
        static constexpr std::size_t kMaxModes = 512;

        /// @brief Holds one mode: its resonator and its shape at the source and the ears.
        struct Mode
        {
            /// @brief Stores b0, b1, b2, a1, a2 of a resonator at the mode's frequency and damping.
            std::array<double, 5> section;

            /// @brief Stores psi_n at the cabinet.
            float source_gain;

            /// @brief Stores psi_n at the left and the right ear.
            std::array<float, 2> ear_gain;
        };

        /// @brief Stores the modes; entries past count are never read.
        std::array<Mode, kMaxModes> modes;

        /// @brief Counts the modes in use.
        std::uint32_t count;

        /// @brief Stores f_s in hertz; zero means the layer is off.
        float crossover_hz;

        /// @brief Stores the scale that matches the layer to Eyring at the seam.
        float level_scale;
    };
}
