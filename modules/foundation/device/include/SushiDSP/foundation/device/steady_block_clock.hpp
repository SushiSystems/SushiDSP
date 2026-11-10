/**************************************************************************/
/* steady_block_clock.hpp                                                 */
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
 * @file steady_block_clock.hpp
 * @brief Declares the wall-clock BlockClock that BlockMonitor uses by default.
 * @author Mustafa Garip
 * @see modules/foundation/device/README.md
 */

#pragma once

#include <chrono>
#include <cstdint>

#include <SushiDSP/foundation/device/block_clock.hpp>

namespace SushiDSP
{
    /// @brief A BlockClock that reads std::chrono::steady_clock.
    class SteadyBlockClock final : public BlockClock
    {
        public:
            /// @brief Returns the steady clock's time in nanoseconds.
            [[nodiscard]] std::int64_t now_nanoseconds() const noexcept override
            {
                return std::chrono::duration_cast<std::chrono::nanoseconds>(
                           std::chrono::steady_clock::now().time_since_epoch())
                    .count();
            }
    };
} // namespace SushiDSP
