/**************************************************************************/
/* diode_pair_port_adapters.hpp                                           */
/**************************************************************************/
/*                          This file is part of:                         */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                         https://sushisystems.io                        */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License"); you    */
/* may not use this file except in compliance with the License. You may   */
/* obtain a copy of the License at                                        */
/*                                                                        */
/*     https://www.apache.org/licenses/LICENSE-2.0                        */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

/**
 * @file diode_pair_port_adapters.hpp
 * @brief Declares the in-memory adapters of the clip stage's nonlinear diode-pair port.
 * @author Mustafa Garip
 */

#pragma once

#include <algorithm>
#include <array>
#include <cmath>

#include <SushiDSP/foundation/circuits/models/silicon_diode.hpp>

namespace SushiDSP::tube_screamer_circuits
{
    /// @brief A diode pair nine times stiffer than a 1N914, with the port's evaluation trail recorded.
    ///
    /// n*Vt = 5 mV instead of 45.3 mV is not a real part; it is what makes Newton run out of its
    /// eight-iteration budget on the clip block, which is the only way to reach DkCircuit's failure
    /// path. previous_voltage and last_voltage let a caller say whether the solver's returned
    /// iterate was somewhere other than its last evaluation.
    struct StiffDiodePairPort
    {
            SiliconDiode diode{2.52e-9, 1.0, 5.0e-3};

            std::array<double, 2> last_voltage{};     ///< Port voltage of the most recent call.
            std::array<double, 2> previous_voltage{}; ///< Port voltage of the call before that.

            /// @brief Evaluates both diode currents and the diagonal Jacobian, recording the trail.
            /// @param v Branch voltages in volts: v[0] across D1, v[1] across D2.
            /// @param i Receives the branch currents in amperes.
            /// @param j Receives the 2x2 Jacobian in siemens; only the diagonal is written.
            void currents_and_jacobian(const std::array<double, 2>& v, std::array<double, 2>& i,
                                       std::array<std::array<double, 2>, 2>& j)
            {
                diode.current_and_jacobian(v[0], i[0], j[0][0]);
                diode.current_and_jacobian(v[1], i[1], j[1][1]);
                previous_voltage = last_voltage;
                last_voltage = v;
            }
    };

    /// @brief A diode pair port that records what DkCircuit's update hook is handed, stamping f(v)
    /// unchanged.
    ///
    /// AdaaDiodePairPort documents a precondition it cannot check on its own: the pair's second
    /// branch is the negative of its first all the way through p, v and F. This port makes the
    /// departure visible without changing what the block computes.
    struct RecordingDiodePairPort
    {
            SiliconDiode diode{};

            double worst_open_circuit_asymmetry = 0.0; ///< Worst |p0 + p1| / |p0| seen.
            double worst_voltage_asymmetry = 0.0;      ///< Worst |v0 + v1| / |v0| seen.
            double worst_impedance_asymmetry = 0.0;    ///< Worst departure of F from antisymmetry.
            double last_impedance = 0.0;               ///< F[0][0] of the most recent call.

            /// @brief Evaluates both diode currents and the diagonal Jacobian at one port voltage.
            /// @param v Branch voltages in volts: v[0] across D1, v[1] across D2.
            /// @param i Receives the branch currents in amperes.
            /// @param j Receives the 2x2 Jacobian in siemens; only the diagonal is written.
            void currents_and_jacobian(const std::array<double, 2>& v, std::array<double, 2>& i,
                                       std::array<std::array<double, 2>, 2>& j) const
            {
                diode.current_and_jacobian(v[0], i[0], j[0][0]);
                diode.current_and_jacobian(v[1], i[1], j[1][1]);
            }

            /// @brief Records the operating point and leaves the stamped currents alone.
            /// @param p The port's open-circuit voltage.
            /// @param v The solved port voltage.
            /// @param f The port impedance matrix.
            /// @param i The branch currents, left as Newton produced them.
            void currents_for_update(const std::array<double, 2>& p, const std::array<double, 2>& v,
                                     const std::array<std::array<double, 2>, 2>& f,
                                     std::array<double, 2>& i)
            {
                static_cast<void>(i);
                const double open_scale = std::fabs(p[0]) + 1e-30;
                const double voltage_scale = std::fabs(v[0]) + 1e-30;
                const double impedance_scale = std::fabs(f[0][0]) + 1e-30;
                worst_open_circuit_asymmetry =
                    std::max(worst_open_circuit_asymmetry, std::fabs(p[0] + p[1]) / open_scale);
                worst_voltage_asymmetry =
                    std::max(worst_voltage_asymmetry, std::fabs(v[0] + v[1]) / voltage_scale);
                worst_impedance_asymmetry = std::max(
                    {worst_impedance_asymmetry, std::fabs(f[0][1] + f[0][0]) / impedance_scale,
                     std::fabs(f[1][0] + f[0][0]) / impedance_scale,
                     std::fabs(f[1][1] - f[0][0]) / impedance_scale});
                last_impedance = f[0][0];
            }
    };
}
