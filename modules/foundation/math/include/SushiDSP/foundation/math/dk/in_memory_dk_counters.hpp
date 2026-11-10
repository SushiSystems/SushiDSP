/**************************************************************************/
/* in_memory_dk_counters.hpp                                              */
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
 * @file in_memory_dk_counters.hpp
 * @brief Declares the in-memory solver counters: the statistics of a DkCircuit, held as plain numbers.
 * @author Mustafa Garip
 */

#pragma once

#include <cstdint>

namespace SushiDSP::dk
{
    /// @brief The statistics of DkCircuit, fixed by the caller.
    ///
    /// It answers the four statistics accessors of DkCircuit and reset_statistics(), so the code
    /// that reads a circuit's counters runs on it without solving a circuit.
    class InMemoryDkCounters final
    {
        public:
            /// @param total_samples The samples processed.
            /// @param non_converged_samples The samples whose Newton iteration did not converge.
            /// @param maximum_iterations_seen The worst iteration count over any one sample.
            /// @param nan_fallback_samples The samples that fell back to the previous state.
            InMemoryDkCounters(std::uint64_t total_samples, std::uint64_t non_converged_samples,
                               std::uint32_t maximum_iterations_seen, std::uint64_t nan_fallback_samples)
                : total_samples_(total_samples), non_converged_samples_(non_converged_samples),
                  maximum_iterations_seen_(maximum_iterations_seen), nan_fallback_samples_(nan_fallback_samples)
            {
            }

            /// @brief Returns the samples processed since the last reset.
            std::uint64_t total_samples() const
            {
                return total_samples_;
            }

            /// @brief Returns the samples whose Newton iteration did not converge since the last reset.
            std::uint64_t non_converged_samples() const
            {
                return non_converged_samples_;
            }

            /// @brief Returns the worst iteration count over any one sample since the last reset.
            std::uint32_t maximum_iterations_seen() const
            {
                return maximum_iterations_seen_;
            }

            /// @brief Returns the samples that fell back to the previous state since the last reset.
            std::uint64_t nan_fallback_samples() const
            {
                return nan_fallback_samples_;
            }

            /// @brief Clears every counter.
            void reset_statistics()
            {
                total_samples_ = 0;
                non_converged_samples_ = 0;
                maximum_iterations_seen_ = 0;
                nan_fallback_samples_ = 0;
            }

        private:
            std::uint64_t total_samples_;
            std::uint64_t non_converged_samples_;
            std::uint32_t maximum_iterations_seen_;
            std::uint64_t nan_fallback_samples_;
    };
} // namespace SushiDSP::dk
