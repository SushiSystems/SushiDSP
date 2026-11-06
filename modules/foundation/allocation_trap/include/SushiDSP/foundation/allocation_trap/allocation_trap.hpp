/**************************************************************************/
/* allocation_trap.hpp                                                    */
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
 * @file allocation_trap.hpp
 * @brief Declares the trap that counts the calling thread's heap allocations.
 * @author Mustafa Garip
 */

#pragma once

/// @brief Holds the test-side code that the test programs of every tier share.
namespace SushiDSP::test
{
    /// @brief Counts the calling thread's heap allocations over the window a case opens, and
    /// closes that window on the way out however the case leaves it.
    class AllocationTrap final
    {
        public:
            /// @brief Zeroes the calling thread's count and leaves the trap closed.
            AllocationTrap() noexcept;

            /// @brief Closes the window, so a case that returns early cannot poison a later one.
            ~AllocationTrap();

            AllocationTrap(const AllocationTrap&) = delete;
            AllocationTrap& operator=(const AllocationTrap&) = delete;
            AllocationTrap(AllocationTrap&&) = delete;
            AllocationTrap& operator=(AllocationTrap&&) = delete;

            /// @brief Opens the window, counting every allocation this thread makes from here.
            void arm() noexcept;

            /// @brief Closes the window.
            void disarm() noexcept;

            /// @brief Reports how many allocations this thread made inside the window.
            [[nodiscard]] int count() const noexcept;
    };
} // namespace SushiDSP::test
