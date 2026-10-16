/**************************************************************************/
/* default_logger.cpp                                                     */
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
 * @file default_logger.cpp
 * @brief Builds the logger of the process on first use and stops its thread at exit.
 * @author Mustafa Garip
 */

#include <SushiDSP/foundation/log/default_logger.hpp>

#include <utility>

#include <SushiDSP/foundation/log/sushidsp_log_project.hpp>

namespace SushiDSP
{
    namespace
    {
        /// @brief Delivers the queue of the logger of the process at static destruction.
        class LoggerShutdown final
        {
            public:
                /// @brief Remembers the logger to stop.
                explicit LoggerShutdown(Logger& logger) noexcept : logger_(logger)
                {
                }

                /// @brief Delivers what is queued and stops the logger thread.
                ~LoggerShutdown()
                {
                    logger_.shutdown();
                }

                LoggerShutdown(const LoggerShutdown&) = delete;
                LoggerShutdown& operator=(const LoggerShutdown&) = delete;
                LoggerShutdown(LoggerShutdown&&) = delete;
                LoggerShutdown& operator=(LoggerShutdown&&) = delete;

            private:
                /// @brief Refers to the logger of the process.
                Logger& logger_;
        };
    } // namespace

    Logger& default_logger() noexcept
    {
        // Never deleted, so a log call stays valid during static initialisation and exit.
        static Logger* const logger = new Logger(nullptr, LogLevel::Warn,
                                                 LogDelivery::Asynchronous,
                                                 K_SUSHIDSP_LOG_PROJECT.level_variable());
        static const LoggerShutdown stopper(*logger);
        return *logger;
    }

    void set_log_level(LogProject project, LogLevel level) noexcept
    {
        if (project.name() == K_SUSHIDSP_LOG_PROJECT.name())
            default_logger().set_log_level(level);
    }

    LogLevel log_level(LogProject project) noexcept
    {
        Logger& logger = default_logger();
        // Reading a category first applies SD_LOG_LEVEL, as the interface asks.
        (void)logger.is_enabled(LogLevel::Off, make_log_category(project, ""));
        return logger.log_level();
    }

    void set_log_level(LogCategory category, LogLevel level) noexcept
    {
        default_logger().set_log_level(category, level);
    }

    LogLevel log_level(LogCategory category) noexcept
    {
        Logger& logger = default_logger();
        (void)logger.is_enabled(LogLevel::Off, category);
        return logger.log_level(category);
    }

    void set_log_sink(LogSink sink)
    {
        default_logger().set_log_sink(std::move(sink));
    }

    void set_log_synchronous(bool synchronous) noexcept
    {
        default_logger().set_delivery(synchronous ? LogDelivery::Synchronous
                                                  : LogDelivery::Asynchronous);
    }

    LogSink log_forward_sink() noexcept
    {
        return LogSink(
            [](const LogRecord& record)
            {
                default_logger().forward(record);
            });
    }
} // namespace SushiDSP