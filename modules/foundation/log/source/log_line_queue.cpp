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

#include <SushiDSP/foundation/log/log_line_queue.hpp>

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

        /// @brief Copies a name into a queued field, cut at K_MAX_LOG_NAME_LENGTH.
        std::size_t copy_name(char* out, std::string_view name) noexcept
        {
            const std::size_t length = std::min(name.size(), K_MAX_LOG_NAME_LENGTH);
            if (length != 0)
            {
                std::memcpy(out, name.data(), length);
            }
            return length;
        }
    } // namespace

    LogRecord QueuedLogLine::record() const noexcept
    {
        const std::string_view whole(text, length);
        return LogRecord{level,
                         std::string_view(project, project_length),
                         std::string_view(module, module_length),
                         std::string_view(file, file_length),
                         line,
                         thread,
                         time_ns,
                         whole.substr(message_offset),
                         whole};
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

        QueuedLogLine& stored = cell->line;
        stored.level = record.level;
        stored.line = record.line;
        stored.thread = record.thread;
        stored.time_ns = record.time_ns;
        stored.project_length = copy_name(stored.project, record.project);
        stored.module_length = copy_name(stored.module, record.module);
        stored.file_length = copy_name(stored.file, record.file);
        stored.length = std::min(record.text.size(), K_MAX_LOG_LINE_LENGTH);
        stored.message_offset =
            record.message.size() <= stored.length ? stored.length - record.message.size() : 0;
        if (stored.length != 0)
        {
            std::memcpy(stored.text, record.text.data(), stored.length);
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

        const QueuedLogLine& stored = cell.line;
        out.level = stored.level;
        out.line = stored.line;
        out.thread = stored.thread;
        out.time_ns = stored.time_ns;
        out.project_length =
            copy_name(out.project, std::string_view(stored.project, stored.project_length));
        out.module_length =
            copy_name(out.module, std::string_view(stored.module, stored.module_length));
        out.file_length =
            copy_name(out.file, std::string_view(stored.file, stored.file_length));
        out.length = stored.length;
        out.message_offset = stored.message_offset;
        std::memcpy(out.text, stored.text, stored.length);
        cell.sequence.store(dequeue_position_ + mask_ + 1, std::memory_order_release);
        ++dequeue_position_;
        return true;
    }

    std::size_t LogLineQueue::capacity() const noexcept
    {
        return mask_ + 1;
    }
} // namespace SushiDSP
