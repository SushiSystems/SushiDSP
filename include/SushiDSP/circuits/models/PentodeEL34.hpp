/****************************************************************/
/* PentodeEL34.hpp                                              */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

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
    /// i[1] = Ig2, i[2] = Ip. The atan(Vpk / KVB) factor makes the plate curve knee over instead
    /// of climbing like a triode's, and is where class-AB crossover behaviour comes from.
    /// current_and_jacobian() carries the physics and current()/jacobian() forward to it. Every
    /// entry point is allocation-free, throw-free and safe on the audio thread.
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
            const double vg2k = v[1] < kScreenFloorVolts ? kScreenFloorVolts : v[1];
            const double vpk = v[2];

            // Below the floor the screen voltage the model sees no longer varies with v[1], so the
            // whole Vg2k column of the Jacobian is zero there. The clamp makes the column
            // one-sided at exactly v[1] == kScreenFloorVolts: the derivative reported at that
            // point is the right-hand one, the unclamped column, while the left-hand one is zero.
            // A 1 mV screen supply is nonphysical, so no operating point sits on that seam; the
            // clamp exists only to keep vg1k / vg2k finite for a solver that wanders there.
            const double screen_follows_input = v[1] < kScreenFloorVolts ? 0.0 : 1.0;

            const double u = kKp * (1.0 / kMu + vg1k / vg2k);                       // u_arg
            double s = 0.0;                                                         // s
            double sigma = 0.0;                                                     // sigma
            models::softplus_and_logistic(u, s, sigma);
            const double e1 = vg2k * s / kKp;                                       // E1
            const double knee = std::atan(vpk / kKvb);                              // knee
            const double z = vg2k / kMu + vg1k;                                     // z

            double grid_softplus = 0.0;
            double grid_logistic = 0.0;
            models::softplus_and_logistic((vg1k - kGridThresholdVolts) / kGridTransitionVolts,
                                          grid_softplus, grid_logistic);

            const double de1_dvg1k = sigma;                                         // dE1/dVg1k
            const double de1_dvg2k = s / kKp - sigma * vg1k / vg2k;                 // dE1/dVg2k

            // kx > 1, so E1^(kx-1) and z^(kx-1) both vanish continuously as their arguments reach
            // zero: the two max(0, .) clamps are C1, not slope discontinuities, and the single
            // guard on each power carries its whole branch, current and derivative alike.
            const double e1_power = e1 <= 0.0 ? 0.0 : std::pow(e1, kKx - 1.0);      // E1^(kx-1)
            const double z_power = z <= 0.0 ? 0.0 : std::pow(z, kKx - 1.0);         // z^(kx-1)

            const double plate_scale = 2.0 * e1_power * e1 / kKg1;                  // 2 E1^kx / kg1
            const double dip_de1 = 2.0 * kKx * e1_power / kKg1 * knee;
            const double dig2_dvg1k = kKx * z_power / kKg2;

            i[0] = kGridCurrentScale * grid_softplus;                               // Ig1
            i[1] = z_power * z / kKg2;                                              // Ig2
            i[2] = plate_scale * knee;                                              // Ip

            j[0][0] = (kGridCurrentScale / kGridTransitionVolts) * grid_logistic;
            j[0][1] = 0.0;
            j[0][2] = 0.0;

            j[1][0] = dig2_dvg1k;
            j[1][1] = dig2_dvg1k / kMu * screen_follows_input;
            j[1][2] = 0.0;

            j[2][0] = dip_de1 * de1_dvg1k;                                          // dIp/dVg1k
            j[2][1] = dip_de1 * de1_dvg2k * screen_follows_input;                   // dIp/dVg2k
            j[2][2] = plate_scale * (1.0 / kKvb)
                      / (1.0 + (vpk / kKvb) * (vpk / kKvb));                        // dIp/dVpk
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
        void jacobian(const std::array<double, 3>& v, std::array<std::array<double, 3>, 3>& j) const
        {
            std::array<double, 3> discarded_currents{};
            current_and_jacobian(v, discarded_currents, j);
        }
    };
}
