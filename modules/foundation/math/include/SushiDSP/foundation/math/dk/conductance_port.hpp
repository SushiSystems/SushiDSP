/**************************************************************************/
/* conductance_port.hpp                                                   */
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
 * @file conductance_port.hpp
 * @brief The nonlinear port whose every branch is one linear conductance, for a DK block stepped without a device model.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <array>
#include <cstddef>

namespace SushiDSP::dk
{
    /// @brief A nonlinear port of N branches, each a linear conductance: i = g v, and dI/dV = g on the diagonal.
    ///
    /// It keeps the glue contract DkCircuit::process_sample steps: currents_and_jacobian() fills every
    /// branch current and the Jacobian's diagonal, and leaves the off-diagonal entries DkCircuit zeroed.
    /// A block stepped with it takes the Newton path with an exactly linear law.
    template <std::size_t N> struct ConductancePort
    {
            /// @brief Holds the conductance of every branch, in siemens.
            double conductance;

            /// @brief Evaluates every branch current and the diagonal Jacobian.
            /// @param v Branch voltages.
            /// @param i Branch currents, overwritten.
            /// @param j Jacobian, its diagonal overwritten.
            void currents_and_jacobian(const std::array<double, N>& v, std::array<double, N>& i,
                                       std::array<std::array<double, N>, N>& j) const
            {
                for (std::size_t branch = 0; branch < N; ++branch)
                {
                    i[branch] = conductance * v[branch];
                    j[branch][branch] = conductance;
                }
            }
    };
} // namespace SushiDSP::dk
