/**************************************************************************/
/* default_logger.hpp                                                     */
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
 * @file default_logger.hpp
 * @brief Declares the one logger of the process and the calls that configure it.
 * @author Mustafa Garip
 */

#pragma once

#include <SushiDSP/foundation/log/log_project.hpp>
#include <SushiDSP/foundation/log/log_sink.hpp>
#include <SushiDSP/foundation/log/logger.hpp>

namespace SushiDSP
{
    /// @brief Returns the logger of the process, the one the SD_LOG macros write through.
    /// It starts at LogLevel::Warn with a StderrLogSink and a logger thread, reads SD_LOG_LEVEL at
    /// its first call, and is never destroyed: at static destruction it delivers what is queued.
    Logger& default_logger() noexcept;

    /// @brief Sets every category of one project; this logger holds SushiDSP's only.
    void set_log_level(LogProject project, LogLevel level) noexcept;

    /// @brief Returns the level of a project's categories that no module setting overrides.
    [[nodiscard]] LogLevel log_level(LogProject project) noexcept;

    /// @brief Sets one category.
    void set_log_level(LogCategory category, LogLevel level) noexcept;

    /// @brief Returns one category's level.
    [[nodiscard]] LogLevel log_level(LogCategory category) noexcept;

    /// @brief Replaces the sink; an empty sink restores stderr.
    void set_log_sink(LogSink sink);

    /// @brief True waits for the queue, then delivers each record on the calling thread.
    void set_log_synchronous(bool synchronous) noexcept;

    /// @brief Returns a sink that enters another logger's record into this logger's queue.
    [[nodiscard]] LogSink log_forward_sink() noexcept;
} // namespace SushiDSP