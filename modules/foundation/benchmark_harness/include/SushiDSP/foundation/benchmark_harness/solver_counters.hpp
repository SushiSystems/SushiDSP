/**************************************************************************/
/* solver_counters.hpp                                                    */
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
 * @file solver_counters.hpp
 * @brief Declares the line a solver case prints under its row.
 * @author Mustafa Garip
 */

#pragma once

#include <cstdint>
#include <cstdio>

namespace SushiDSP::bench
{
    /// @brief Prints one circuit's solve, non-convergence, worst-iteration and NaN counters.
    /// @tparam Circuit A type with the counters of SushiDSP::dk::DkCircuit.
    /// @see modules/foundation/benchmark_harness/README.md for why a cost needs its counters.
    template <typename Circuit> void print_solver_counters(const Circuit& circuit)
    {
        const std::uint64_t solves = circuit.total_samples();
        const std::uint64_t non_converged = circuit.non_converged_samples();
        std::printf(
            "  solver: %llu solves, %llu non-converged (%.4f %%), %u iterations worst "
            "case, %llu NaN fallbacks\n",
            static_cast<unsigned long long>(solves), static_cast<unsigned long long>(non_converged),
            solves == 0 ? 0.0
                        : 100.0 * static_cast<double>(non_converged) / static_cast<double>(solves),
            circuit.maximum_iterations_seen(),
            static_cast<unsigned long long>(circuit.nan_fallback_samples()));
    }
} // namespace SushiDSP::bench
