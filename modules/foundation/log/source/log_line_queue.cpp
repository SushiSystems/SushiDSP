/**************************************************************************/
/* log_line_queue.cpp                                                     */
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
 * @file log_line_queue.cpp
 * @brief Implements the bounded multi-producer, single-consumer queue of log lines.
 * @author Mustafa Garip
 */

#include "SushiDSP/log/log_line_queue.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>

namespace SushiDSP
{
    namespace
    {
        /// @brief Holds the smallest ring the turn counters can tell full from empty in.
        constexpr std::size_t K_MINIMUM_CAPACITY = 2;

        /// @brief Returns the smallest power of two that is at least a value.
        std::size_t round_up_to_power_of_two(std::size_t value) noexcept
        {
            std::size_t power = K_MINIMUM_CAPACITY;
            while (power < value)
            {
                power <<= 1;
            }
            return power;
        }
    } // namespace

    LogRecord QueuedLogLine::record() const noexcept
    {
        return LogRecord{level, category, std::string_view(text, length)};
    }

    LogLineQueue::LogLineQueue(std::size_t capacity)
        : mask_(round_up_to_power_of_two(capacity) - 1), cells_(new Cell[mask_ + 1])
    {
        for (std::size_t index = 0; index <= mask_; ++index)
        {
            cells_[index].sequence.store(index, std::memory_order_relaxed);
        }
    }

    bool LogLineQueue::push(const LogRecord& record) noexcept
    {
        std::size_t position = enqueue_position_.load(std::memory_order_relaxed);
        Cell* cell = nullptr;

        for (;;)
        {
            cell = &cells_[position & mask_];
            const std::size_t sequence = cell->sequence.load(std::memory_order_acquire);
            const std::intptr_t lead =
                static_cast<std::intptr_t>(sequence) - static_cast<std::intptr_t>(position);
            if (lead < 0)
            {
                return false;
            }
            // A slot whose sequence equals the position is free and this producer may claim it.
            if (lead == 0 && enqueue_position_.compare_exchange_weak(position, position + 1,
                                                                     std::memory_order_relaxed))
            {
                break;
            }
            if (lead > 0)
            {
                position = enqueue_position_.load(std::memory_order_relaxed);
            }
        }

        cell->line.level = record.level;
        cell->line.category = record.category;
        cell->line.length = std::min(record.message.size(), K_MAX_LOG_MESSAGE_LENGTH);
        if (cell->line.length != 0)
        {
            std::memcpy(cell->line.text, record.message.data(), cell->line.length);
        }
        cell->sequence.store(position + 1, std::memory_order_release);
        return true;
    }

    bool LogLineQueue::pop(QueuedLogLine& out) noexcept
    {
        Cell& cell = cells_[dequeue_position_ & mask_];
        if (cell.sequence.load(std::memory_order_acquire) != dequeue_position_ + 1)
        {
            return false;
        }

        out.level = cell.line.level;
        out.category = cell.line.category;
        out.length = cell.line.length;
        std::memcpy(out.text, cell.line.text, cell.line.length);

        cell.sequence.store(dequeue_position_ + mask_ + 1, std::memory_order_release);
        ++dequeue_position_;
        return true;
    }

    std::size_t LogLineQueue::capacity() const noexcept
    {
        return mask_ + 1;
    }
} // namespace SushiDSP
