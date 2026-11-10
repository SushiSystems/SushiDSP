/**************************************************************************/
/* nanosecond_clock.hpp                                                   */
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
 * @file nanosecond_clock.hpp
 * @brief Declares the clock port the unrecorded timing path reads, and the steady-clock adapter.
 * @author Mustafa Garip
 */

#pragma once

#include <chrono>

namespace SushiDSP::bench
{
    /// @brief Reads the steady clock; the adapter every benchmark run uses.
    ///
    /// A clock is any type with `double now_nanoseconds()`, the port measure_unrecorded() reads.
    /// It is a template argument, not a virtual interface, so the read adds no indirect call to
    /// the repetition it times.
    class SteadyNanosecondClock final
    {
        public:
            /// @brief Returns the time since an unspecified origin, in nanoseconds.
            double now_nanoseconds() const
            {
                const auto since_origin = std::chrono::steady_clock::now().time_since_epoch();
                return std::chrono::duration<double, std::nano>(since_origin).count();
            }
    };
} // namespace SushiDSP::bench
