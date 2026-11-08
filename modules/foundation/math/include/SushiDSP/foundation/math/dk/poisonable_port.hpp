/**************************************************************************/
/* poisonable_port.hpp                                                    */
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
 * @file poisonable_port.hpp
 * @brief The one-branch nonlinear port that returns a non-finite current on demand, for a DK block's fault path.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <array>
#include <limits>

namespace SushiDSP::dk
{
    /// @brief A nonlinear port of one linear branch that returns an infinite current while poisoned.
    ///
    /// Healthy, it is a ConductancePort of one branch. Poisoned, its current is infinity and its
    /// Jacobian still the conductance, so DkCircuit meets a non-finite solve without an unstable circuit.
    struct PoisonablePort
    {
            /// @brief Holds the branch conductance, in siemens.
            double conductance;
            /// @brief Holds whether the branch current is infinity.
            bool poisoned = false;

            /// @brief Evaluates the branch current, or infinity while poisoned, and the 1x1 Jacobian.
            /// @param v Branch voltages.
            /// @param i Branch currents, overwritten.
            /// @param j Jacobian, overwritten.
            void currents_and_jacobian(const std::array<double, 1>& v, std::array<double, 1>& i,
                                       std::array<std::array<double, 1>, 1>& j) const
            {
                i[0] = poisoned ? std::numeric_limits<double>::infinity() : conductance * v[0];
                j[0][0] = conductance;
            }
    };
} // namespace SushiDSP::dk
