/**************************************************************************/
/* Triode12AX7.hpp                                                        */
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
 * @file Triode12AX7.hpp
 * @brief Defines the stateless Koren 12AX7 triode model with Norman's grid law.
 * @author Mustafa Garip
 */

#pragma once

#include <array>
#include <cmath>
#include <cstddef>

#include "SushiDSP/circuits/models/SoftTransition.hpp"

namespace SushiDSP
{
    /// @brief Stateless Koren 12AX7 triode: plate current plus Norman's soft grid-conduction law.
    ///
    /// Parameters are transcribed from docs/reference/spice/models/12ax7_koren.sub. The two branch
    /// voltages are v[0] = Vgk and v[1] = Vpk; the two branch currents are i[0] = Ig and i[1] = Ip.
    /// A DK solver wants the currents and the Jacobian at the same operating point, so
    /// current_and_jacobian() carries the physics and current()/jacobian() forward to it. Every
    /// entry point is allocation-free, throw-free and safe on the audio thread.
    class Triode12AX7
    {
    public:
        /// Branch 0 is the grid, branch 1 is the plate.
        static constexpr std::size_t kBranchCount = 2;

        /// Amplification factor MU (12ax7_koren.sub).
        static constexpr double kMu = 100.0;

        /// Koren's grid-drive sharpness KP (12ax7_koren.sub).
        static constexpr double kKp = 600.0;

        /// Koren's knee-softening constant KVB (12ax7_koren.sub).
        static constexpr double kKvb = 300.0;

        /// Koren's perveance divisor KG1 (12ax7_koren.sub).
        static constexpr double kKg1 = 1060.0;

        /// Koren's plate-current exponent KX (12ax7_koren.sub).
        static constexpr double kKx = 1.4;

        /// Norman grid-current scale IG0, in amperes (12ax7_koren.sub).
        static constexpr double kGridCurrentScale = 1.0e-6;

        /// Norman grid-conduction threshold VGAMMA, in volts (12ax7_koren.sub).
        static constexpr double kGridThresholdVolts = 0.5;

        /// Norman grid-conduction transition width VTG, in volts (12ax7_koren.sub).
        static constexpr double kGridTransitionVolts = 0.15;

        /// @brief Evaluates both branch currents and the 2x2 Jacobian at one operating point.
        ///
        /// The plate law's root, softplus, logistic and E1^(kx-1) are shared between the current
        /// and its derivative, and E1^kx is formed as E1^(kx-1) * E1 rather than by a second pow.
        /// current() and jacobian() forward here.
        /// @param v Branch voltages: v[0] = Vgk, v[1] = Vpk, in volts.
        /// @param i Receives the branch currents: i[0] = Ig, i[1] = Ip, in amperes.
        /// @param j Receives the 2x2 Jacobian j[a][b] = d i_a / d v_b in siemens.
        void current_and_jacobian(const std::array<double, 2>& v, std::array<double, 2>& i,
                                  std::array<std::array<double, 2>, 2>& j) const
        {
            const double vgk = v[0];
            const double vpk = v[1];

            const double root = std::sqrt(kKvb + vpk * vpk);                        // root
            const double t = kKp * (1.0 / kMu + vgk / root);                        // t
            double s = 0.0;                                                         // s
            double sigma = 0.0;                                                     // sigma
            models::softplus_and_logistic(t, s, sigma);
            const double e1 = vpk * s / kKp;                                        // E1

            double grid_softplus = 0.0;
            double grid_logistic = 0.0;
            models::softplus_and_logistic((vgk - kGridThresholdVolts) / kGridTransitionVolts,
                                          grid_softplus, grid_logistic);

            const double de1_dvgk = vpk * sigma / root;                             // dE1/dVgk
            const double de1_dvpk = s / kKp
                                    - vpk * vpk * vgk * sigma / (root * root * root);  // dE1/dVpk

            // kx > 1, so Ip and its derivatives vanish continuously at E1 = 0 under this one guard.
            const double e1_power = e1 <= 0.0 ? 0.0 : std::pow(e1, kKx - 1.0);      // E1^(kx-1)
            const double dip_de1 = 2.0 * kKx * e1_power / kKg1;

            i[0] = kGridCurrentScale * grid_softplus;                               // Ig
            i[1] = 2.0 * e1_power * e1 / kKg1;                                      // Ip

            j[0][0] = (kGridCurrentScale / kGridTransitionVolts) * grid_logistic;
            j[0][1] = 0.0;
            j[1][0] = dip_de1 * de1_dvgk;                                           // dIp/dVgk
            j[1][1] = dip_de1 * de1_dvpk;                                           // dIp/dVpk
        }

        /// @brief Evaluates the grid and plate currents at one operating point.
        /// @param v Branch voltages: v[0] = Vgk, v[1] = Vpk, in volts.
        /// @param i Receives the branch currents: i[0] = Ig, i[1] = Ip, in amperes.
        void current(const std::array<double, 2>& v, std::array<double, 2>& i) const
        {
            std::array<std::array<double, 2>, 2> discarded_jacobian{};
            current_and_jacobian(v, i, discarded_jacobian);
        }

        /// @brief Evaluates the branch-current Jacobian j[a][b] = d i_a / d v_b at one point.
        /// @param v Branch voltages: v[0] = Vgk, v[1] = Vpk, in volts.
        /// @param j Receives the 2x2 Jacobian in siemens; j[0][1] is exactly zero because the grid
        ///          current depends on Vgk alone.
        void jacobian(const std::array<double, 2>& v, std::array<std::array<double, 2>, 2>& j) const
        {
            std::array<double, 2> discarded_currents{};
            current_and_jacobian(v, discarded_currents, j);
        }
    };
}
