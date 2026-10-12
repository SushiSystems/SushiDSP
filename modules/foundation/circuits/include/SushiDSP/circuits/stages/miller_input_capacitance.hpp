/**************************************************************************/
/* miller_input_capacitance.hpp                                           */
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
 * @file miller_input_capacitance.hpp
 * @brief A driven grid's input capacitance and the pole its driving resistance makes with it.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

namespace SushiDSP
{
    /// @brief Holds the three capacitances a triode grid presents and turns them, with the
    /// resistance driving that grid, into the interstage pole the grid-plate term dominates once
    /// the stage's gain multiplies it.
    struct MillerInputCapacitance
    {
            /// @brief Carries the grid-cathode capacitance, in picofarads, which the gain leaves alone.
            float grid_cathode_picofarads;

            /// @brief Carries the grid-plate capacitance, in picofarads, which one plus the stage's
            /// gain multiplies.
            float grid_plate_picofarads;

            /// @brief Carries the wiring capacitance at the grid node, in picofarads.
            float stray_wiring_picofarads;

            /// @brief Returns the corner of the pole this capacitance makes with its driving resistance.
            /// @param stage_voltage_gain The driven stage's mid-band gain.
            /// @param driving_resistance_ohms The resistance the grid is driven through.
            /// @return The -3 dB corner, in hertz. Finite for any positive resistance.
            double corner_hz(double stage_voltage_gain, double driving_resistance_ohms) const
            {
                const double input_capacitance_farads =
                    (grid_cathode_picofarads + grid_plate_picofarads * (1.0 + stage_voltage_gain) +
                     stray_wiring_picofarads) *
                    1e-12;
                return 1.0 / (2.0 * K_PI * driving_resistance_ohms * input_capacitance_farads);
            }

        private:
            /// @brief Carries pi to the precision <cmath>'s M_PI holds.
            static constexpr double K_PI = 3.14159265358979323846;
    };

    /// @brief The 12AX7's grid capacitances, the two from
    /// docs/reference/spice/models/12ax7_koren.sub and the 10 pF a point-to-point turret board
    /// strays at the grid node.
    inline constexpr MillerInputCapacitance K_TRIODE12_AX7_MILLER_CAPACITANCE{1.6f, 1.7f, 10.0f};
} // namespace SushiDSP
