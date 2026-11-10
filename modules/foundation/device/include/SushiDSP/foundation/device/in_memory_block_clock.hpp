/**************************************************************************/
/* in_memory_block_clock.hpp                                              */
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
 * @file in_memory_block_clock.hpp
 * @brief Declares the in-memory BlockClock, whose time moves only when the caller advances it.
 * @author Mustafa Garip
 * @see modules/foundation/device/README.md
 */

#pragma once

#include <atomic>
#include <cstdint>

#include <SushiDSP/foundation/device/block_clock.hpp>

namespace SushiDSP
{
    /// @brief A BlockClock that starts at zero and moves only by advance().
    ///
    /// It never reads the wall clock, so a test places a block's arrival and end exactly.
    class InMemoryBlockClock final : public BlockClock
    {
        public:
            /// @brief Returns the time set so far.
            [[nodiscard]] std::int64_t now_nanoseconds() const noexcept override
            {
                return now_.load(std::memory_order_acquire);
            }

            /// @brief Moves the time forward by @p nanoseconds.
            void advance(std::int64_t nanoseconds) noexcept
            {
                now_.fetch_add(nanoseconds, std::memory_order_acq_rel);
            }

        private:
            /// @brief The time in nanoseconds.
            std::atomic<std::int64_t> now_{0};
    };
} // namespace SushiDSP
