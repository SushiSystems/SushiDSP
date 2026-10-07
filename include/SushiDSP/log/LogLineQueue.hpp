/**************************************************************************/
/* LogLineQueue.hpp                                                       */
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
 * @file LogLineQueue.hpp
 * @brief Declares the bounded queue that carries log lines to the asynchronous sink's worker.
 * @author Mustafa Garip
 */

#pragma once

#include <atomic>
#include <cstddef>
#include <memory>

#include "SushiDSP/log/LogRecord.hpp"

namespace SushiDSP
{
    /// @brief Holds one line by value while it waits in the queue.
    struct QueuedLogLine
    {
        LogLevel    level = LogLevel::Trace;           /**< The severity of the line. */
        LogCategory category = make_log_category("");  /**< The module the line comes from. */
        std::size_t length = 0;                        /**< The characters of text in use. */
        char        text[kMaxLogMessageLength];        /**< The message, with no terminator. */

        /// @brief Returns the line as the record a sink takes; it points into this object.
        LogRecord record() const noexcept;
    };

    /// @brief Carries lines from any number of producer threads to one consumer thread.
    /// push() and pop() neither block nor allocate; the storage is taken once, at construction.
    class LogLineQueue final
    {
    public:
        /// @brief Allocates the storage for every line the queue can hold.
        /// @param capacity Rounded up to a power of two, and to at least two.
        explicit LogLineQueue(std::size_t capacity);

        LogLineQueue(const LogLineQueue&) = delete;
        LogLineQueue& operator=(const LogLineQueue&) = delete;
        LogLineQueue(LogLineQueue&&) = delete;
        LogLineQueue& operator=(LogLineQueue&&) = delete;
        ~LogLineQueue() = default;

        /// @brief Copies a line into the queue. Any thread may call it, several at once.
        /// @return False when the queue is full; the line is then not stored.
        [[nodiscard]] bool push(const LogRecord& record) noexcept;

        /// @brief Moves the oldest line out of the queue. One thread only.
        /// @return False when the queue is empty; out is then untouched.
        [[nodiscard]] bool pop(QueuedLogLine& out) noexcept;

        /// @brief Returns how many lines the queue holds when full.
        std::size_t capacity() const noexcept;

    private:
        /// @brief Holds one slot of the ring and the turn counter that guards it.
        struct Cell
        {
            std::atomic<std::size_t> sequence{ 0 }; /**< The position allowed to use the slot. */
            QueuedLogLine            line;          /**< The stored line. */
        };

        /// @brief Holds the capacity minus one; the capacity is a power of two.
        std::size_t mask_;

        /// @brief Holds the slots of the ring.
        std::unique_ptr<Cell[]> cells_;

        /// @brief Counts the lines producers have claimed a slot for.
        std::atomic<std::size_t> enqueue_position_{ 0 };

        /// @brief Counts the lines the consumer has taken; only the consumer touches it.
        std::size_t dequeue_position_ = 0;
    };
}
