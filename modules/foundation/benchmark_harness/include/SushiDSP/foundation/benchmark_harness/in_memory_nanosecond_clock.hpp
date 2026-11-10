/**************************************************************************/
/* in_memory_nanosecond_clock.hpp                                         */
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
 * @file in_memory_nanosecond_clock.hpp
 * @brief Declares the in-memory clock: every read advances it by a fixed step.
 * @author Mustafa Garip
 */

#pragma once

namespace SushiDSP::bench
{
    /// @brief A clock held in memory that moves by a fixed step on every read.
    ///
    /// It keeps the promise of the nanosecond clock port: successive reads never decrease. A start
    /// read and a stop read around one call therefore differ by exactly the step.
    class StepNanosecondClock final
    {
        public:
            /// @param step_nanoseconds How far each read advances the clock.
            explicit StepNanosecondClock(double step_nanoseconds) : step_(step_nanoseconds)
            {
            }

            /// @brief Returns the current time, then advances the clock by one step.
            double now_nanoseconds()
            {
                const double now = now_;
                now_ += step_;
                return now;
            }

        private:
            double step_;
            double now_ = 0.0;
    };
} // namespace SushiDSP::bench
