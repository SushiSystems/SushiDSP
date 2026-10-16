/**************************************************************************/
/* test_logger.cpp                                                        */
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
 * @file test_logger.cpp
 * @brief Tests the logger's per-category levels, its environment setting, its delivery and sinks.
 * @author Mustafa Garip
 */

#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/log/async_log_sink.hpp>
#include <SushiDSP/foundation/log/capturing_log_sink.hpp>
#include <SushiDSP/foundation/log/log.hpp>
#include <SushiDSP/foundation/log/sushidsp_log_project.hpp>
#include <SushiDSP/foundation/log/log_thread.hpp>
#include <SushiDSP/foundation/log/sushidsp_log_project.hpp>
#include <SushiDSP/foundation/log/scoped_log_sink.hpp>
#include <SushiDSP/foundation/log/sushidsp_log_project.hpp>

namespace
{
    /// @brief Names the lines this file logs.
    inline constexpr SushiDSP::LogCategory K_LOGGER_TEST_LOG_CATEGORY =
        SushiDSP::make_log_category(SushiDSP::K_SUSHIDSP_LOG_PROJECT, "logger_test");

    /// @brief Names a second category, so levels can differ between two.
    inline constexpr SushiDSP::LogCategory K_OTHER_TEST_LOG_CATEGORY =
        SushiDSP::make_log_category(SushiDSP::K_SUSHIDSP_LOG_PROJECT, "other_test");

    /// @brief Returns the site of a call made from this file.
    SushiDSP::LogSite here(int line)
    {
        return SushiDSP::LogSite{__FILE__, line};
    }

    /// @brief Throws on every write, the way a broken sink would.
    class ThrowingLogSink final : public SushiDSP::LogRecordSink
    {
        public:
            /// @brief Throws a runtime_error naming the sink.
            void write(const SushiDSP::LogRecord&) override
            {
                throw std::runtime_error("disk full");
            }

            /// @brief Does nothing.
            void flush() noexcept override
            {
            }
    };

    /// @brief Sets an environment variable for the length of a scope.
    class ScopedEnvironment final
    {
        public:
            /// @brief Sets the variable.
            ScopedEnvironment(const char* name, const char* value) : name_(name)
            {
#if defined(_WIN32)
                _putenv_s(name, value);
#else
                setenv(name, value, 1);
#endif
            }

            /// @brief Removes the variable.
            ~ScopedEnvironment()
            {
#if defined(_WIN32)
                _putenv_s(name_, "");
#else
                unsetenv(name_);
#endif
            }

            ScopedEnvironment(const ScopedEnvironment&) = delete;
            ScopedEnvironment& operator=(const ScopedEnvironment&) = delete;
            ScopedEnvironment(ScopedEnvironment&&) = delete;
            ScopedEnvironment& operator=(ScopedEnvironment&&) = delete;

        private:
            /// @brief Names the variable to remove.
            const char* name_;
    };
} // namespace

TEST(Small_Logger, LoggerHandsTheSinkTheLevelCategoryMessageAndWholeLine)
{
    const auto capture = std::make_shared<SushiDSP::CapturingLogSink>();
    SushiDSP::Logger logger(capture, SushiDSP::LogLevel::Trace, SushiDSP::LogDelivery::Synchronous);

    logger.log(SushiDSP::LogLevel::Warn, K_LOGGER_TEST_LOG_CATEGORY, here(97), "slot {} refused '{}'",
               3, "cab.wav");

    const std::vector<SushiDSP::CapturedLogLine> lines = capture->lines();
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0].level, SushiDSP::LogLevel::Warn);
    EXPECT_EQ(lines[0].category, "logger_test");
    EXPECT_EQ(lines[0].message, "slot 3 refused 'cab.wav'");
    EXPECT_NE(lines[0].line.find("] [WARN] [sushidsp.logger_test] [t"), std::string::npos) << lines[0].line;
    EXPECT_NE(lines[0].line.find("] [test_logger.cpp:97] slot 3 refused 'cab.wav'"),
              std::string::npos)
        << lines[0].line;
}

TEST(Small_Logger, LoggerStartsEveryCategoryAtTheLevelItWasGiven)
{
    SushiDSP::Logger logger(nullptr, SushiDSP::LogLevel::Warn, SushiDSP::LogDelivery::Synchronous);

    EXPECT_EQ(logger.log_level(K_LOGGER_TEST_LOG_CATEGORY), SushiDSP::LogLevel::Warn);
    EXPECT_FALSE(logger.is_enabled(SushiDSP::LogLevel::Info, K_LOGGER_TEST_LOG_CATEGORY));
    EXPECT_TRUE(logger.is_enabled(SushiDSP::LogLevel::Warn, K_LOGGER_TEST_LOG_CATEGORY));
}

TEST(Small_Logger, LoggerFiltersEachCategoryAtItsOwnLevel)
{
    const auto capture = std::make_shared<SushiDSP::CapturingLogSink>();
    SushiDSP::Logger logger(capture, SushiDSP::LogLevel::Warn, SushiDSP::LogDelivery::Synchronous);
    logger.set_log_level(K_LOGGER_TEST_LOG_CATEGORY, SushiDSP::LogLevel::Debug);

    logger.log(SushiDSP::LogLevel::Debug, K_LOGGER_TEST_LOG_CATEGORY, here(122), "own debug");
    logger.log(SushiDSP::LogLevel::Debug, K_OTHER_TEST_LOG_CATEGORY, here(123), "other debug");
    logger.log(SushiDSP::LogLevel::Warn, K_OTHER_TEST_LOG_CATEGORY, here(124), "other warn");

    const std::vector<SushiDSP::CapturedLogLine> lines = capture->lines();
    ASSERT_EQ(lines.size(), 2u);
    EXPECT_EQ(lines[0].message, "own debug");
    EXPECT_EQ(lines[1].message, "other warn");
}

TEST(Small_Logger, LoggerSetsEveryCategoryAtOnceTheOwnLevelsIncluded)
{
    SushiDSP::Logger logger(nullptr, SushiDSP::LogLevel::Warn, SushiDSP::LogDelivery::Synchronous);
    logger.set_log_level(K_LOGGER_TEST_LOG_CATEGORY, SushiDSP::LogLevel::Trace);

    logger.set_log_level(SushiDSP::LogLevel::Error);

    EXPECT_EQ(logger.log_level(K_LOGGER_TEST_LOG_CATEGORY), SushiDSP::LogLevel::Error);
    EXPECT_EQ(logger.log_level(K_OTHER_TEST_LOG_CATEGORY), SushiDSP::LogLevel::Error);
}

TEST(Small_Logger, LoggerOffPassesNothingNotEvenFatal)
{
    SushiDSP::Logger logger(nullptr, SushiDSP::LogLevel::Off, SushiDSP::LogDelivery::Synchronous);

    EXPECT_FALSE(logger.is_enabled(SushiDSP::LogLevel::Fatal, K_LOGGER_TEST_LOG_CATEGORY));
}

TEST(Small_Logger, LoggerReadsItsLevelsFromTheEnvironmentAtTheFirstLogCall)
{
    const ScopedEnvironment environment("SD_LOGGER_TEST_LEVEL", "error, Logger_Test = TRACE");
    SushiDSP::Logger logger(nullptr, SushiDSP::LogLevel::Warn, SushiDSP::LogDelivery::Synchronous,
                            "SD_LOGGER_TEST_LEVEL");
    EXPECT_EQ(logger.log_level(K_OTHER_TEST_LOG_CATEGORY), SushiDSP::LogLevel::Warn);

    EXPECT_TRUE(logger.is_enabled(SushiDSP::LogLevel::Trace, K_LOGGER_TEST_LOG_CATEGORY));
    EXPECT_EQ(logger.log_level(K_OTHER_TEST_LOG_CATEGORY), SushiDSP::LogLevel::Error);
}

TEST(Small_Logger, LoggerLetsACallMadeBeforeTheFirstLogCallWinOverTheEnvironment)
{
    const ScopedEnvironment environment("SD_LOGGER_TEST_LEVEL", "warn,logger_test=trace,other_test=error");
    SushiDSP::Logger logger(nullptr, SushiDSP::LogLevel::Warn, SushiDSP::LogDelivery::Synchronous,
                            "SD_LOGGER_TEST_LEVEL");
    logger.set_log_level(K_LOGGER_TEST_LOG_CATEGORY, SushiDSP::LogLevel::Info);

    EXPECT_FALSE(logger.is_enabled(SushiDSP::LogLevel::Debug, K_LOGGER_TEST_LOG_CATEGORY));
    EXPECT_EQ(logger.log_level(K_LOGGER_TEST_LOG_CATEGORY), SushiDSP::LogLevel::Info);
    EXPECT_EQ(logger.log_level(K_OTHER_TEST_LOG_CATEGORY), SushiDSP::LogLevel::Error);
}

TEST(Small_Logger, LoggerKeepsItsLevelsAndReportsOnceWhenTheEnvironmentCannotBeRead)
{
    const ScopedEnvironment environment("SD_LOGGER_TEST_LEVEL", "loud");
    SushiDSP::Logger logger(nullptr, SushiDSP::LogLevel::Warn, SushiDSP::LogDelivery::Synchronous,
                            "SD_LOGGER_TEST_LEVEL");

    testing::internal::CaptureStderr();
    EXPECT_FALSE(logger.is_enabled(SushiDSP::LogLevel::Info, K_LOGGER_TEST_LOG_CATEGORY));
    EXPECT_FALSE(logger.is_enabled(SushiDSP::LogLevel::Info, K_OTHER_TEST_LOG_CATEGORY));
    const std::string reported = testing::internal::GetCapturedStderr();

    EXPECT_EQ(reported, "[log] SD_LOGGER_TEST_LEVEL='loud' cannot be read; the levels stay as they are\n");
    EXPECT_EQ(logger.log_level(K_LOGGER_TEST_LOG_CATEGORY), SushiDSP::LogLevel::Warn);
}

TEST(Small_Logger, MacroBelowTheRuntimeLevelDoesNotEvaluateItsArguments)
{
    const auto capture = std::make_shared<SushiDSP::CapturingLogSink>();
    SushiDSP::Logger& logger = SushiDSP::default_logger();
    const SushiDSP::LogLevel level_before = logger.log_level(K_LOGGER_TEST_LOG_CATEGORY);
    int evaluations = 0;
    {
        const SushiDSP::ScopedLogSink guard(logger, capture);
        logger.set_log_level(K_LOGGER_TEST_LOG_CATEGORY, SushiDSP::LogLevel::Error);

        SD_LOG_WARN(K_LOGGER_TEST_LOG_CATEGORY, "warn {}", ++evaluations);
        EXPECT_EQ(evaluations, 0);

        SD_LOG_ERROR(K_LOGGER_TEST_LOG_CATEGORY, "error {}", ++evaluations);
        EXPECT_EQ(evaluations, 1);
    }
    logger.set_log_level(K_LOGGER_TEST_LOG_CATEGORY, level_before);

    const std::vector<SushiDSP::CapturedLogLine> lines = capture->lines();
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0].level, SushiDSP::LogLevel::Error);
    EXPECT_EQ(lines[0].message, "error 1");
    EXPECT_NE(lines[0].line.find("[test_logger.cpp:"), std::string::npos) << lines[0].line;
}

TEST(Small_Logger, LoggerCutsAMessageAtTheMaximumLengthAndKeepsTheWholeLine)
{
    const auto capture = std::make_shared<SushiDSP::CapturingLogSink>();
    SushiDSP::Logger logger(capture, SushiDSP::LogLevel::Trace, SushiDSP::LogDelivery::Synchronous);
    const std::string twice_the_limit(2 * SushiDSP::K_MAX_LOG_MESSAGE_LENGTH, 'a');

    logger.log(SushiDSP::LogLevel::Info, K_LOGGER_TEST_LOG_CATEGORY, here(214), "{}", twice_the_limit);

    const std::vector<SushiDSP::CapturedLogLine> lines = capture->lines();
    ASSERT_EQ(lines.size(), 1u);
    const std::string kept(SushiDSP::K_MAX_LOG_MESSAGE_LENGTH, 'a');
    EXPECT_EQ(lines[0].message, kept);
    ASSERT_GT(lines[0].line.size(), kept.size());
    EXPECT_EQ(lines[0].line.substr(lines[0].line.size() - kept.size()), kept);
}

TEST(Small_Logger, LoggerFlushesTheSinkAfterAFatalLine)
{
    const auto capture = std::make_shared<SushiDSP::CapturingLogSink>();
    SushiDSP::Logger logger(capture, SushiDSP::LogLevel::Trace, SushiDSP::LogDelivery::Synchronous);

    logger.log(SushiDSP::LogLevel::Error, K_LOGGER_TEST_LOG_CATEGORY, here(229), "error");
    EXPECT_EQ(capture->flush_count(), 0u);

    logger.log(SushiDSP::LogLevel::Fatal, K_LOGGER_TEST_LOG_CATEGORY, here(232), "fatal");
    EXPECT_EQ(capture->flush_count(), 1u);
}

TEST(Small_Logger, LoggerWritesTheExceptionAndTheRecordToStderrWhenTheSinkThrows)
{
    SushiDSP::Logger logger(std::make_shared<ThrowingLogSink>(), SushiDSP::LogLevel::Trace,
                            SushiDSP::LogDelivery::Synchronous);

    testing::internal::CaptureStderr();
    logger.log(SushiDSP::LogLevel::Error, K_LOGGER_TEST_LOG_CATEGORY, here(242), "lost {}", 7);
    const std::string reported = testing::internal::GetCapturedStderr();

    EXPECT_EQ(reported.rfind("[log] the sink threw 'disk full'; the record follows\n", 0), 0u)
        << reported;
    EXPECT_NE(reported.find("[ERROR] [sushidsp.logger_test]"), std::string::npos) << reported;
    EXPECT_NE(reported.find("[test_logger.cpp:242] lost 7\n"), std::string::npos) << reported;
}

TEST(Small_Logger, LoggerPutsStderrBackWhenANullSinkIsSet)
{
    const auto capture = std::make_shared<SushiDSP::CapturingLogSink>();
    SushiDSP::Logger logger(capture, SushiDSP::LogLevel::Trace, SushiDSP::LogDelivery::Synchronous);

    EXPECT_EQ(logger.set_sink(nullptr), capture);
    testing::internal::CaptureStderr();
    logger.log(SushiDSP::LogLevel::Warn, K_LOGGER_TEST_LOG_CATEGORY, here(257), "to stderr");
    const std::string written = testing::internal::GetCapturedStderr();

    EXPECT_TRUE(capture->lines().empty());
    EXPECT_NE(written.find("[test_logger.cpp:257] to stderr\n"), std::string::npos) << written;
}

TEST(Medium_Logger, AsynchronousLoggerDeliversOnItsOwnThreadInOrderBeforeFlushReturns)
{
    constexpr int K_LINES = static_cast<int>(SushiDSP::K_DEFAULT_LOG_QUEUE_CAPACITY) - 1;
    const auto capture = std::make_shared<SushiDSP::CapturingLogSink>();
    SushiDSP::Logger logger(capture, SushiDSP::LogLevel::Trace, SushiDSP::LogDelivery::Asynchronous);

    for (int index = 0; index < K_LINES; ++index)
        logger.log(SushiDSP::LogLevel::Info, K_LOGGER_TEST_LOG_CATEGORY, here(270), "line {}", index);
    logger.flush();

    const std::vector<SushiDSP::CapturedLogLine> lines = capture->lines();
    ASSERT_EQ(lines.size(), static_cast<std::size_t>(K_LINES));
    for (int index = 0; index < K_LINES; ++index)
        EXPECT_EQ(lines[index].message, "line " + std::to_string(index));
    EXPECT_EQ(logger.dropped_count(), 0u);
}

TEST(Medium_Logger, SwitchingToSynchronousDeliveryFirstDeliversTheQueue)
{
    const auto capture = std::make_shared<SushiDSP::CapturingLogSink>();
    SushiDSP::Logger logger(capture, SushiDSP::LogLevel::Trace, SushiDSP::LogDelivery::Asynchronous);
    logger.log(SushiDSP::LogLevel::Info, K_LOGGER_TEST_LOG_CATEGORY, here(284), "queued");

    logger.set_delivery(SushiDSP::LogDelivery::Synchronous);
    logger.log(SushiDSP::LogLevel::Info, K_LOGGER_TEST_LOG_CATEGORY, here(287), "direct");

    const std::vector<SushiDSP::CapturedLogLine> lines = capture->lines();
    ASSERT_EQ(lines.size(), 2u);
    EXPECT_EQ(lines[0].message, "queued");
    EXPECT_EQ(lines[1].message, "direct");
    EXPECT_EQ(logger.delivery(), SushiDSP::LogDelivery::Synchronous);
}

TEST(Medium_Logger, AsynchronousFatalIsDeliveredAfterTheQueueOnTheCallingThreadAndFlushed)
{
    const auto capture = std::make_shared<SushiDSP::CapturingLogSink>();
    SushiDSP::Logger logger(capture, SushiDSP::LogLevel::Trace, SushiDSP::LogDelivery::Asynchronous);
    logger.log(SushiDSP::LogLevel::Info, K_LOGGER_TEST_LOG_CATEGORY, here(299), "before");

    logger.log(SushiDSP::LogLevel::Fatal, K_LOGGER_TEST_LOG_CATEGORY, here(301), "cannot go on");

    const std::vector<SushiDSP::CapturedLogLine> lines = capture->lines();
    ASSERT_EQ(lines.size(), 2u);
    EXPECT_EQ(lines[0].message, "before");
    EXPECT_EQ(lines[1].message, "cannot go on");
    EXPECT_GE(capture->flush_count(), 1u);
}

TEST(Medium_Logger, ShutdownDeliversTheQueueAndLaterRecordsGoOnTheCallingThread)
{
    const auto capture = std::make_shared<SushiDSP::CapturingLogSink>();
    SushiDSP::Logger logger(capture, SushiDSP::LogLevel::Trace, SushiDSP::LogDelivery::Asynchronous);
    logger.log(SushiDSP::LogLevel::Info, K_LOGGER_TEST_LOG_CATEGORY, here(313), "queued");

    logger.shutdown();
    logger.log(SushiDSP::LogLevel::Info, K_LOGGER_TEST_LOG_CATEGORY, here(316), "after");

    const std::vector<SushiDSP::CapturedLogLine> lines = capture->lines();
    ASSERT_EQ(lines.size(), 2u);
    EXPECT_EQ(lines[0].message, "queued");
    EXPECT_EQ(lines[1].message, "after");
}

TEST(Medium_Logger, RecordsOfTwoThreadsNameTwoThreadNumbers)
{
    const auto capture = std::make_shared<SushiDSP::CapturingLogSink>();
    SushiDSP::Logger logger(capture, SushiDSP::LogLevel::Trace, SushiDSP::LogDelivery::Synchronous);

    logger.log(SushiDSP::LogLevel::Info, K_LOGGER_TEST_LOG_CATEGORY, here(328), "main");
    std::thread worker(
        [&logger]
        {
            logger.log(SushiDSP::LogLevel::Info, K_LOGGER_TEST_LOG_CATEGORY, here(332), "worker");
        });
    worker.join();

    const std::vector<SushiDSP::CapturedLogLine> lines = capture->lines();
    ASSERT_EQ(lines.size(), 2u);
    const std::string thread = "[t" + std::to_string(SushiDSP::current_log_thread_number()) + "]";
    EXPECT_NE(lines[0].line.find(thread), std::string::npos) << lines[0].line;
    EXPECT_EQ(lines[1].line.find(thread), std::string::npos) << lines[1].line;
}

TEST(Small_Logger, LogCeilingCutsOnlyTraceInAnOptimisedBuild)
{
#if defined(NDEBUG)
    EXPECT_EQ(SD_LOG_CEILING, SD_LOG_LEVEL_DEBUG);
#else
    EXPECT_EQ(SD_LOG_CEILING, SD_LOG_LEVEL_TRACE);
#endif
}