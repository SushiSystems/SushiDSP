/**************************************************************************/
/* log_thread.cpp                                                         */
/**************************************************************************/
/*                          This file is part of:                         */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                         https://sushisystems.io                        */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the PolyForm Noncommercial License 1.0.0 (the           */
/* "License"); you may not use this file except in compliance with the    */
/* License. You may obtain a copy of the License at                       */
/*                                                                        */
/*     https://polyformproject.org/licenses/noncommercial/1.0.0           */
/*                                                                        */
/* Noncommercial use is free. Commercial use requires a separate licence  */
/* from Sushi Systems; see COMMERCIAL.md. The software is provided        */
/* "as is", without warranty of any kind.                                 */
/**************************************************************************/

/**
 * @file log_thread.cpp
 * @brief Implements the per-thread log number.
 * @author Mustafa Garip
 */

#include <SushiDSP/foundation/log/log_thread.hpp>

#include <atomic>

namespace SushiDSP
{
    namespace
    {
        /// @brief Holds the number the next thread to ask is given.
        std::atomic<std::uint32_t> next_log_thread_number{1};
    } // namespace

    std::uint32_t current_log_thread_number() noexcept
    {
        thread_local const std::uint32_t number =
            next_log_thread_number.fetch_add(1, std::memory_order_relaxed);
        return number;
    }
} // namespace SushiDSP
