/**************************************************************************/
/* log_level_table.cpp                                                    */
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
 * @file log_level_table.cpp
 * @brief Implements the per-category level table.
 * @author Mustafa Garip
 */

#include <SushiDSP/foundation/log/log_level_table.hpp>

namespace SushiDSP
{
    LogLevelTable::LogLevelTable(LogLevel every) noexcept : every_(every)
    {
    }

    LogLevel LogLevelTable::level_of(std::uint64_t category_hash) const noexcept
    {
        const Slot* slot = find(category_hash);
        return slot == nullptr ? every_.load(std::memory_order_relaxed)
                               : slot->level.load(std::memory_order_relaxed);
    }

    LogLevel LogLevelTable::shared_level() const noexcept
    {
        return every_.load(std::memory_order_relaxed);
    }

    void LogLevelTable::set_every(LogLevel level) noexcept
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        every_.store(level, std::memory_order_relaxed);
        for (Slot& slot : slots_)
        {
            if (slot.hash.load(std::memory_order_acquire) != 0)
                slot.level.store(level, std::memory_order_relaxed);
        }
    }

    void LogLevelTable::set_shared(LogLevel level) noexcept
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        every_.store(level, std::memory_order_relaxed);
    }

    bool LogLevelTable::set(std::uint64_t category_hash, LogLevel level) noexcept
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        Slot* slot = find_or_claim(category_hash);
        if (slot == nullptr)
            return false;
        slot->level.store(level, std::memory_order_relaxed);
        return true;
    }

    bool LogLevelTable::has_own_level(std::uint64_t category_hash) const noexcept
    {
        return find(category_hash) != nullptr;
    }

    const LogLevelTable::Slot* LogLevelTable::find(std::uint64_t category_hash) const noexcept
    {
        for (std::size_t probe = 0; probe < K_MAX_LOG_CATEGORIES; ++probe)
        {
            const Slot& slot = slots_[(category_hash + probe) % K_MAX_LOG_CATEGORIES];
            const std::uint64_t held = slot.hash.load(std::memory_order_acquire);
            if (held == category_hash)
                return &slot;
            // Slots are claimed in probe order and never freed, so a free slot ends the search.
            if (held == 0)
                return nullptr;
        }
        return nullptr;
    }

    LogLevelTable::Slot* LogLevelTable::find_or_claim(std::uint64_t category_hash) noexcept
    {
        for (std::size_t probe = 0; probe < K_MAX_LOG_CATEGORIES; ++probe)
        {
            Slot& slot = slots_[(category_hash + probe) % K_MAX_LOG_CATEGORIES];
            const std::uint64_t held = slot.hash.load(std::memory_order_relaxed);
            if (held == category_hash)
                return &slot;
            if (held == 0)
            {
                // The level is stored before the hash is released, so a reader never sees it unset.
                slot.level.store(every_.load(std::memory_order_relaxed), std::memory_order_relaxed);
                slot.hash.store(category_hash, std::memory_order_release);
                return &slot;
            }
        }
        return nullptr;
    }
} // namespace SushiDSP
