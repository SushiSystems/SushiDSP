/**************************************************************************/
/* block_clock.hpp                                                        */
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
 * @file block_clock.hpp
 * @brief Declares the port through which BlockMonitor reads the time.
 * @author Mustafa Garip
 * @see modules/foundation/device/README.md
 */

#pragma once

#include <cstdint>

namespace SushiDSP
{
    /// @brief The time source of a BlockMonitor.
    ///
    /// The audio thread calls now_nanoseconds() twice per block, so it must not allocate, lock or
    /// block. Only differences between two readings mean anything; the zero point is the clock's own.
    class BlockClock
    {
        public:
            virtual ~BlockClock() = default;

            /// @brief Returns the time in nanoseconds since the clock's own zero point.
            [[nodiscard]] virtual std::int64_t now_nanoseconds() const noexcept = 0;
    };
} // namespace SushiDSP
