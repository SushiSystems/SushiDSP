/**************************************************************************/
/* pentode_el34.hpp                                                       */
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
 * @file pentode_el34.hpp
 * @brief Defines the stateless Koren EL34 pentode model with Norman's grid law.
 * @author Mustafa Garip
 */

#pragma once

#include <array>
#include <cmath>
#include <cstddef>

#include <SushiDSP/foundation/circuits/models/soft_transition.hpp>

namespace SushiDSP
{
    /// @brief Stateless Koren EL34 pentode: plate and screen currents plus Norman's grid law.
    ///
    /// Parameters are transcribed from docs/reference/spice/models/el34_koren.sub. The three branch
    /// voltages are v[0] = Vg1k, v[1] = Vg2k, v[2] = Vpk; the branch currents are i[0] = Ig1,
    /// i[1] = Ig2, i[2] = Ip. The atan(Vpk / KVB) factor makes the plate curve knee over instead
    /// of climbing like a triode's, and is where class-AB crossover behaviour comes from.
    /// current_and_jacobian() carries the physics and current()/jacobian() forward to it. Every
    /// entry point is allocation-free, throw-free and safe on the audio thread.
    class PentodeEL34
    {
        public:
            /// Branch 0 is the control grid, branch 1 the screen grid, branch 2 the plate.
            static constexpr std::size_t K_BRANCH_COUNT = 3;

            /// Amplification factor MU (el34_koren.sub).
            static constexpr double K_MU = 11.0;

            /// Koren's grid-drive sharpness KP (el34_koren.sub).
            static constexpr double K_KP = 60.0;

            /// Koren's plate-knee constant KVB (el34_koren.sub).
            static constexpr double K_KVB = 24.0;

            /// Koren's plate perveance divisor KG1 (el34_koren.sub).
            static constexpr double K_KG1 = 650.0;

            /// Koren's screen perveance divisor KG2 (el34_koren.sub).
            static constexpr double K_KG2 = 4500.0;

            /// Koren's current exponent KX, shared by the plate and screen laws (el34_koren.sub).
            static constexpr double K_KX = 1.35;

            /// Norman grid-current scale IG0, in amperes (el34_koren.sub).
            static constexpr double K_GRID_CURRENT_SCALE = 1.0e-6;

            /// Norman grid-conduction threshold VGAMMA, in volts (el34_koren.sub).
            static constexpr double K_GRID_THRESHOLD_VOLTS = 0.5;

            /// Norman grid-conduction transition width VTG, in volts (el34_koren.sub).
            static constexpr double K_GRID_TRANSITION_VOLTS = 0.15;

            /// Lower bound applied to Vg2k before it is divided into, in volts.
            static constexpr double K_SCREEN_FLOOR_VOLTS = 1.0e-3;

            /// @brief Evaluates all three branch currents and the 3x3 Jacobian at one operating point.
            ///
            /// The screen clamp, softplus, logistic, E1, the plate knee, z and the two (kx-1) powers
            /// are shared between the currents and their derivatives; E1^kx and z^kx are formed from
            /// those powers rather than by a second pow. current() and jacobian() forward here.
            /// @param v Branch voltages: v[0] = Vg1k, v[1] = Vg2k, v[2] = Vpk, in volts.
            /// @param i Receives the branch currents: i[0] = Ig1, i[1] = Ig2, i[2] = Ip, in amperes.
            /// @param j Receives the 3x3 Jacobian j[a][b] = d i_a / d v_b in siemens.
            void current_and_jacobian(const std::array<double, 3>& v, std::array<double, 3>& i,
                                      std::array<std::array<double, 3>, 3>& j) const
            {
                const double vg1k = v[0];
                const double vg2k = v[1] < K_SCREEN_FLOOR_VOLTS ? K_SCREEN_FLOOR_VOLTS : v[1];
                const double vpk = v[2];

                // The Vg2k Jacobian column is zero below the screen floor (modules/foundation/circuits/README.md).
                const double screen_follows_input = v[1] < K_SCREEN_FLOOR_VOLTS ? 0.0 : 1.0;

                const double u = K_KP * (1.0 / K_MU + vg1k / vg2k); // u_arg
                double s = 0.0;                                     // s
                double sigma = 0.0;                                 // sigma
                models::softplus_and_logistic(u, s, sigma);
                const double e1 = vg2k * s / K_KP;          // E1
                const double knee = std::atan(vpk / K_KVB); // knee
                const double z = vg2k / K_MU + vg1k;        // z

                double grid_softplus = 0.0;
                double grid_logistic = 0.0;
                models::softplus_and_logistic((vg1k - K_GRID_THRESHOLD_VOLTS) /
                                                  K_GRID_TRANSITION_VOLTS,
                                              grid_softplus, grid_logistic);

                const double de1_dvg1k = sigma;                          // dE1/dVg1k
                const double de1_dvg2k = s / K_KP - sigma * vg1k / vg2k; // dE1/dVg2k

                // kx > 1, so each guarded power vanishes continuously at zero (modules/foundation/circuits/README.md).
                const double e1_power = e1 <= 0.0 ? 0.0 : std::pow(e1, K_KX - 1.0); // E1^(kx-1)
                const double z_power = z <= 0.0 ? 0.0 : std::pow(z, K_KX - 1.0);    // z^(kx-1)

                const double plate_scale = 2.0 * e1_power * e1 / K_KG1; // 2 E1^kx / kg1
                const double dip_de1 = 2.0 * K_KX * e1_power / K_KG1 * knee;
                const double dig2_dvg1k = K_KX * z_power / K_KG2;

                i[0] = K_GRID_CURRENT_SCALE * grid_softplus; // Ig1
                i[1] = z_power * z / K_KG2;                  // Ig2
                i[2] = plate_scale * knee;                   // Ip

                j[0][0] = (K_GRID_CURRENT_SCALE / K_GRID_TRANSITION_VOLTS) * grid_logistic;
                j[0][1] = 0.0;
                j[0][2] = 0.0;

                j[1][0] = dig2_dvg1k;
                j[1][1] = dig2_dvg1k / K_MU * screen_follows_input;
                j[1][2] = 0.0;

                j[2][0] = dip_de1 * de1_dvg1k;                        // dIp/dVg1k
                j[2][1] = dip_de1 * de1_dvg2k * screen_follows_input; // dIp/dVg2k
                j[2][2] =
                    plate_scale * (1.0 / K_KVB) / (1.0 + (vpk / K_KVB) * (vpk / K_KVB)); // dIp/dVpk
            }

            /// @brief Evaluates the three branch currents at one operating point.
            /// @param v Branch voltages: v[0] = Vg1k, v[1] = Vg2k, v[2] = Vpk, in volts.
            /// @param i Receives the branch currents: i[0] = Ig1, i[1] = Ig2, i[2] = Ip, in amperes.
            void current(const std::array<double, 3>& v, std::array<double, 3>& i) const
            {
                std::array<std::array<double, 3>, 3> discarded_jacobian{};
                current_and_jacobian(v, i, discarded_jacobian);
            }

            /// @brief Evaluates the branch-current Jacobian j[a][b] = d i_a / d v_b at one point.
            /// @param v Branch voltages: v[0] = Vg1k, v[1] = Vg2k, v[2] = Vpk, in volts.
            /// @param j Receives the 3x3 Jacobian in siemens. The control-grid row has only a Vg1k
            ///          entry, and the screen row has no Vpk entry, because neither current sees the
            ///          plate voltage at all.
            void jacobian(const std::array<double, 3>& v,
                          std::array<std::array<double, 3>, 3>& j) const
            {
                std::array<double, 3> discarded_currents{};
                current_and_jacobian(v, discarded_currents, j);
            }
    };
} // namespace SushiDSP
