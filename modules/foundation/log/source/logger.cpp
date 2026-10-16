/**************************************************************************/
/* logger.cpp                                                             */
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
 * @file logger.cpp
 * @brief Implements the logger's levels, its environment setting, its sink and its delivery.
 * @author Mustafa Garip
 */

#include <SushiDSP/foundation/log/logger.hpp>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <optional>
#include <utility>

#include <SushiDSP/foundation/log/async_log_sink.hpp>
#include <SushiDSP/foundation/log/log_level_setting.hpp>
#include <SushiDSP/foundation/log/log_line.hpp>
#include <SushiDSP/foundation/log/log_thread.hpp>
#include <SushiDSP/foundation/log/stderr_log_sink.hpp>

namespace SushiDSP
{
    namespace
    {
        /// @brief Writes a sink's failure and the record it lost to stderr.
        void report_sink_failure(const char* what, const LogRecord& record) noexcept
        {
            const std::string_view text = record.text.empty() ? record.message : record.text;
            std::fprintf(stderr, "[log] the sink threw '%s'; the record follows\n%.*s\n", what,
                         static_cast<int>(text.size()), text.data());
        }

        /// @brief Returns the value of an environment variable, or null when it is not set.
        const char* environment_value(std::string_view name) noexcept
        {
            char terminated[64];
            if (name.empty() || name.size() >= sizeof(terminated))
                return nullptr;
            name.copy(terminated, name.size());
            terminated[name.size()] = '\0';
            return std::getenv(terminated);
        }

        /// @brief Hands each record to a LogSink callable; the callable has nothing to flush.
        class DelegateLogSink final : public LogRecordSink
        {
            public:
                /// @brief Holds the callable.
                explicit DelegateLogSink(LogSink sink) noexcept : sink_(std::move(sink))
                {
                }

                /// @brief Calls the callable with the record.
                void write(const LogRecord& record) override
                {
                    sink_(record);
                }

                /// @brief Does nothing; the callable delivers before it returns.
                void flush() noexcept override
                {

                }

            private:
                /// @brief Holds the callable.
                LogSink sink_;
        };
    } // namespace

    class Logger::CurrentSink final : public LogRecordSink
    {
        public:
            /// @brief Forwards to the sink the logger has in place.
            explicit CurrentSink(Logger& logger) noexcept : logger_(logger)
            {
            }

            /// @brief Writes the record to the logger's sink in place.
            void write(const LogRecord& record) noexcept override
            {
                logger_.write_to_sink(record);
            }

            /// @brief Flushes the logger's sink in place.
            void flush() noexcept override
            {
                logger_.flush_sink();
            }

        private:
            /// @brief Refers to the logger whose sink receives the records.
            Logger& logger_;
    };

    Logger::Logger(std::shared_ptr<LogRecordSink> sink, LogLevel every, LogDelivery delivery,
                   std::string_view environment_variable)
        : levels_(every), environment_variable_(environment_variable),
          stderr_sink_(std::make_shared<StderrLogSink>()),
          sink_(sink ? std::move(sink) : stderr_sink_), delivery_(delivery)
    {
    }

    Logger::~Logger()
    {
        shutdown();
    }

    std::shared_ptr<LogRecordSink> Logger::set_sink(std::shared_ptr<LogRecordSink> sink) noexcept
    {
        if (!sink)
            sink = stderr_sink_;
        const std::lock_guard<std::mutex> lock(sink_mutex_);
        sink_.swap(sink);
        return sink;
    }

    void Logger::set_log_level(LogLevel level) noexcept
    {
        every_set_in_code_.store(true, std::memory_order_relaxed);
        levels_.set_every(level);
    }

    void Logger::set_log_level(const LogCategory& category, LogLevel level) noexcept
    {
        levels_.set(category.hash(), level);
    }

    void Logger::set_log_sink(LogSink sink)
    {
        set_sink(sink ? std::make_shared<DelegateLogSink>(std::move(sink)) : nullptr);
    }

    LogLevel Logger::log_level() const noexcept
    {
        return levels_.shared_level();
    }

    LogLevel Logger::log_level(const LogCategory& category) const noexcept
    {
        return levels_.level_of(category.hash());
    }

    bool Logger::is_enabled(LogLevel level, const LogCategory& category) noexcept
    {
        if (!environment_read_.load(std::memory_order_acquire))
            apply_environment();
        return level >= levels_.level_of(category.hash());
    }

    void Logger::set_delivery(LogDelivery delivery) noexcept
    {
        if (delivery == LogDelivery::Synchronous)
        {
            AsyncLogSink* async = async_started_.load(std::memory_order_acquire);
            if (async != nullptr)
                async->flush();
        }
        delivery_.store(delivery, std::memory_order_release);
    }

    LogDelivery Logger::delivery() const noexcept
    {
        return delivery_.load(std::memory_order_acquire);
    }

    void Logger::write(LogLevel level, const LogCategory& category, LogSite site,
                       std::string_view message) noexcept
    {
        const auto now = std::chrono::system_clock::now();
        deliver_line(level, category.project().name(), category.module(),
                     log_site_file_name(site.file), static_cast<std::uint32_t>(site.line),
                     std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch())
                         .count(),
                     message);
    }

    void Logger::forward(const LogRecord& record) noexcept
    {
        deliver_line(record.level, record.project, record.module, record.file, record.line,
                     record.time_ns, record.message.substr(0, K_MAX_LOG_MESSAGE_LENGTH));
    }

    void Logger::deliver_line(LogLevel level, std::string_view project, std::string_view module,
                              std::string_view file, std::uint32_t line, std::int64_t time_ns,
                              std::string_view message) noexcept
    {
        const std::chrono::system_clock::time_point time(
            std::chrono::duration_cast<std::chrono::system_clock::duration>(
                std::chrono::nanoseconds(time_ns)));
        const std::uint32_t thread = current_log_thread_number();
        char text[K_MAX_LOG_LINE_LENGTH];
        const std::size_t length = format_log_line(
            text, sizeof(text),
            LogLineFields{time, level, project, module, thread, file, line, message});
        const std::string_view whole(text, length);
        deliver(LogRecord{level, project, module, file, line, thread, time_ns,
                          whole.substr(length - message.size()), whole});
    }
    void Logger::flush() noexcept
    {
        AsyncLogSink* async = async_started_.load(std::memory_order_acquire);
        if (async != nullptr)
            async->flush();
        else
            flush_sink();
    }

    void Logger::shutdown() noexcept
    {
        delivery_.store(LogDelivery::Synchronous, std::memory_order_release);
        AsyncLogSink* async = async_started_.load(std::memory_order_acquire);
        if (async != nullptr)
            async->stop();
    }

    std::uint64_t Logger::dropped_count() const noexcept
    {
        const AsyncLogSink* async = async_started_.load(std::memory_order_acquire);
        return async == nullptr ? 0 : async->dropped_count();
    }

    void Logger::apply_environment() noexcept
    {
        std::call_once(environment_once_,
                       [this]
                       {
                           const char* value = environment_value(environment_variable_);
                           if (value != nullptr && !every_set_in_code_.load(std::memory_order_relaxed))
                           {
                               const std::optional<LogLevelSetting> setting =
                                   parse_log_level_setting(value);
                               if (!setting)
                               {
                                   std::fprintf(stderr,
                                                "[log] %.*s='%s' cannot be read; the levels stay "
                                                "as they are\n",
                                                static_cast<int>(environment_variable_.size()),
                                                environment_variable_.data(), value);
                               }
                               else
                               {
                                   levels_.set_shared(setting->every);
                                   for (std::size_t index = 0; index < setting->pair_count; ++index)
                                   {
                                       const LogCategoryLevel& pair = setting->pairs[index];
                                       if (!levels_.has_own_level(pair.category_hash))
                                           levels_.set(pair.category_hash, pair.level);
                                   }
                               }
                           }
                           environment_read_.store(true, std::memory_order_release);
                       });
    }

    void Logger::deliver(const LogRecord& record) noexcept
    {
        AsyncLogSink* async = async_started_.load(std::memory_order_acquire);
        if (record.level == LogLevel::Fatal)
        {
            if (async != nullptr)
                async->flush();
            write_to_sink(record);
            flush_sink();
            return;
        }
        if (delivery_.load(std::memory_order_acquire) == LogDelivery::Synchronous)
        {
            write_to_sink(record);
            return;
        }
        try
        {
            async_sink().write(record);
        }
        catch (const std::exception& error)
        {
            report_sink_failure(error.what(), record);
        }
    }

    void Logger::write_to_sink(const LogRecord& record) noexcept
    {
        const std::shared_ptr<LogRecordSink> sink = current_sink();
        try
        {
            sink->write(record);
        }
        catch (const std::exception& error)
        {
            report_sink_failure(error.what(), record);
        }
        catch (...)
        {
            report_sink_failure("an exception of unknown type", record);
        }
    }

    void Logger::flush_sink() noexcept
    {
        current_sink()->flush();
    }

    AsyncLogSink& Logger::async_sink()
    {
        std::call_once(async_once_,
                       [this]
                       {
                           async_ = std::make_unique<AsyncLogSink>(
                               std::make_shared<CurrentSink>(*this), K_DEFAULT_LOG_QUEUE_CAPACITY);
                           async_started_.store(async_.get(), std::memory_order_release);
                       });
        return *async_;
    }

    std::shared_ptr<LogRecordSink> Logger::current_sink() const noexcept
    {
        const std::lock_guard<std::mutex> lock(sink_mutex_);
        return sink_;
    }
} // namespace SushiDSP
