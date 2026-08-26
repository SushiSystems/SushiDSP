/**************************************************************************/
/* PentodeEL34.hpp                                                        */
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


#pragma once

#include <array>
#include <cmath>
#include <cstddef>

#include "SushiDSP/circuits/models/SoftTransition.hpp"

namespace SushiDSP
{
    /// @brief Stateless Koren EL34 pentode: plate and screen currents plus Norman's grid law.
    ///
    /// Parameters are transcribed from docs/reference/spice/models/el34_koren.sub. The three branch
    /// voltages are v[0] = Vg1k, v[1] = Vg2k, v[2] = Vpk; the branch currents are i[0] = Ig1,
    /// i[1] = Ig2, i[2] = Ip. The atan(Vpk / KVB) factor on the plate current is what makes the
    /// plate curve knee over instead of climbing like a triode's, and is where class-AB crossover
    /// behaviour ultimately comes from. Both entry points are allocation-free, throw-free and safe
    /// on the audio thread.
    class PentodeEL34
    {
    public:
        /// Branch 0 is the control grid, branch 1 the screen grid, branch 2 the plate.
        static constexpr std::size_t kBranchCount = 3;

        /// Amplification factor MU (el34_koren.sub).
        static constexpr double kMu = 11.0;

        /// Koren's grid-drive sharpness KP (el34_koren.sub).
        static constexpr double kKp = 60.0;

        /// Koren's plate-knee constant KVB (el34_koren.sub).
        static constexpr double kKvb = 24.0;

        /// Koren's plate perveance divisor KG1 (el34_koren.sub).
        static constexpr double kKg1 = 650.0;

        /// Koren's screen perveance divisor KG2 (el34_koren.sub).
        static constexpr double kKg2 = 4500.0;

        /// Koren's current exponent KX, shared by the plate and screen laws (el34_koren.sub).
        static constexpr double kKx = 1.35;

        /// Norman grid-current scale IG0, in amperes (el34_koren.sub).
        static constexpr double kGridCurrentScale = 1.0e-6;

        /// Norman grid-conduction threshold VGAMMA, in volts (el34_koren.sub).
        static constexpr double kGridThresholdVolts = 0.5;

        /// Norman grid-conduction transition width VTG, in volts (el34_koren.sub).
        static constexpr double kGridTransitionVolts = 0.15;

        /// Lower bound applied to Vg2k before it is divided into, in volts.
        static constexpr double kScreenFloorVolts = 1.0e-3;

        /// @brief Evaluates the three branch currents at one operating point.
        /// @param v Branch voltages: v[0] = Vg1k, v[1] = Vg2k, v[2] = Vpk, in volts.
        /// @param i Receives the branch currents: i[0] = Ig1, i[1] = Ig2, i[2] = Ip, in amperes.
        void current(const std::array<double, 3>& v, std::array<double, 3>& i) const
        {
            const double vg1k = v[0];
            const double vg2k = v[1] < kScreenFloorVolts ? kScreenFloorVolts : v[1];
            const double vpk = v[2];

            const double u = kKp * (1.0 / kMu + vg1k / vg2k);                       // u_arg
            const double s = models::softplus(u);                                   // s
            const double e1 = vg2k * s / kKp;                                       // E1
            const double knee = std::atan(vpk / kKvb);                              // knee
            const double z = vg2k / kMu + vg1k;                                     // z

            i[0] = kGridCurrentScale
                   * models::softplus((vg1k - kGridThresholdVolts) / kGridTransitionVolts);
            i[1] = z <= 0.0 ? 0.0 : std::pow(z, kKx) / kKg2;                        // Ig2
            i[2] = e1 <= 0.0 ? 0.0 : 2.0 * std::pow(e1, kKx) / kKg1 * knee;         // Ip
        }

        /// @brief Evaluates the branch-current Jacobian j[a][b] = d i_a / d v_b at one point.
        /// @param v Branch voltages: v[0] = Vg1k, v[1] = Vg2k, v[2] = Vpk, in volts.
        /// @param j Receives the 3x3 Jacobian in siemens. The control-grid row has only a Vg1k
        ///          entry, and the screen row has no Vpk entry, because neither current sees the
        ///          plate voltage at all.
        void jacobian(const std::array<double, 3>& v, std::array<std::array<double, 3>, 3>& j) const
        {
            const double vg1k = v[0];
            const double vg2k = v[1] < kScreenFloorVolts ? kScreenFloorVolts : v[1];
            const double vpk = v[2];

            // Below the floor the screen voltage the model sees no longer varies with v[1], so the
            // whole Vg2k column of the Jacobian is genuinely zero there.
            const double screen_follows_input = v[1] < kScreenFloorVolts ? 0.0 : 1.0;

            const double u = kKp * (1.0 / kMu + vg1k / vg2k);                       // u_arg
            const double s = models::softplus(u);                                   // s
            const double sigma = models::logistic(u);                               // sigma
            const double e1 = vg2k * s / kKp;                                       // E1
            const double knee = std::atan(vpk / kKvb);                              // knee
            const double z = vg2k / kMu + vg1k;                                     // z

            const double de1_dvg1k = sigma;                                         // dE1/dVg1k
            const double de1_dvg2k = s / kKp - sigma * vg1k / vg2k;                 // dE1/dVg2k

            // kx > 1, so E1^(kx-1) and z^(kx-1) both vanish continuously as their arguments reach
            // zero: the two max(0, .) clamps are C1, not slope discontinuities.
            const double dip_de1 =
                e1 <= 0.0 ? 0.0 : 2.0 * kKx * std::pow(e1, kKx - 1.0) / kKg1 * knee;
            const double dig2_dvg1k = z <= 0.0 ? 0.0 : kKx * std::pow(z, kKx - 1.0) / kKg2;

            j[0][0] = (kGridCurrentScale / kGridTransitionVolts)
                      * models::logistic((vg1k - kGridThresholdVolts) / kGridTransitionVolts);
            j[0][1] = 0.0;
            j[0][2] = 0.0;

            j[1][0] = dig2_dvg1k;
            j[1][1] = dig2_dvg1k / kMu * screen_follows_input;
            j[1][2] = 0.0;

            j[2][0] = dip_de1 * de1_dvg1k;                                          // dIp/dVg1k
            j[2][1] = dip_de1 * de1_dvg2k * screen_follows_input;                   // dIp/dVg2k
            j[2][2] = e1 <= 0.0
                          ? 0.0
                          : (2.0 * std::pow(e1, kKx) / kKg1) * (1.0 / kKvb)
                                / (1.0 + (vpk / kKvb) * (vpk / kKvb));              // dIp/dVpk
        }
    };
}
