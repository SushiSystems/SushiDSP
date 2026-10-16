/**************************************************************************/
/* log_level_table.hpp                                                    */
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
 * @file log_level_table.hpp
 * @brief Declares the table that holds one atomic level per log category.
 * @author Mustafa Garip
 */

#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>

#include <SushiDSP/foundation/log/log_category.hpp>
#include <SushiDSP/foundation/log/log_level.hpp>

namespace SushiDSP
{
    /// @brief Bounds how many categories can carry a level of their own.
    inline constexpr std::size_t K_MAX_LOG_CATEGORIES = 64;

    /// @brief Holds the level of every category, found by the category's hash.
    /// Reading a level takes no lock and allocates nothing; setting one takes a mutex. A category
    /// with no level of its own reads the level set for every category.
    class LogLevelTable final
    {
        public:
            /// @brief Starts every category at one level.
            explicit LogLevelTable(LogLevel every) noexcept;

            LogLevelTable(const LogLevelTable&) = delete;
            LogLevelTable& operator=(const LogLevelTable&) = delete;
            LogLevelTable(LogLevelTable&&) = delete;
            LogLevelTable& operator=(LogLevelTable&&) = delete;
            ~LogLevelTable() = default;

            /// @brief Returns the level a category passes records at.
            [[nodiscard]] LogLevel level_of(std::uint64_t category_hash) const noexcept;

            /// @brief Sets every category, those with a level of their own included.
            void set_every(LogLevel level) noexcept;

            /// @brief Returns the level of every category with no level of its own.
            [[nodiscard]] LogLevel shared_level() const noexcept;

            /// @brief Sets the level of the categories with no level of their own.
            void set_shared(LogLevel level) noexcept;

            /// @brief Sets one category.
            /// @return False when the table is full and the category keeps the shared level.
            bool set(std::uint64_t category_hash, LogLevel level) noexcept;

            /// @brief Reports whether a category has a level of its own.
            [[nodiscard]] bool has_own_level(std::uint64_t category_hash) const noexcept;

        private:
            /// @brief Holds one category's hash and level; a zero hash marks a free slot.
            struct Slot
            {
                    std::atomic<std::uint64_t> hash{0};          /**< The category. */
                    std::atomic<LogLevel> level{LogLevel::Warn}; /**< Its level. */
            };

            /// @brief Returns the slot of a category, or null when it has none.
            const Slot* find(std::uint64_t category_hash) const noexcept;

            /// @brief Returns the slot of a category, claiming a free one; null when full.
            /// @pre mutex_ is held.
            Slot* find_or_claim(std::uint64_t category_hash) noexcept;

            /// @brief Serialises the writers, so a slot is claimed by one of them only.
            std::mutex mutex_;

            /// @brief Holds the level a category with no slot reads.
            std::atomic<LogLevel> every_;

            /// @brief Holds the slots, probed linearly from hash modulo the size.
            std::array<Slot, K_MAX_LOG_CATEGORIES> slots_;
    };
} // namespace SushiDSP
