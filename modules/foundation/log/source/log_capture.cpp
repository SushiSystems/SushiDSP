/**
 * @file log_capture.cpp
 * @brief Defines the scope that collects every SushiLog record.
 * @author Mustafa Garip
 */

#include <SushiDSP/foundation/log/log_capture.hpp>

#include <SushiLog/log/log_calls.hpp>
#include <SushiLog/log/log_record.hpp>
#include <SushiLog/log/log_sink.hpp>

namespace SushiDSP
{
    LogCapture::LogCapture()
    {
        SushiLog::set_log_synchronous(true);
        SushiLog::set_log_sink(SushiLog::LogSink(
            [this](const SushiLog::LogRecord& record)
            {
                const std::lock_guard<std::mutex> lock(mutex_);
                lines_.push_back(CapturedLogLine{record.level, std::string(record.module),
                                                 std::string(record.message),
                                                 std::string(record.text)});
            }));
    }

    LogCapture::~LogCapture()
    {
        SushiLog::set_log_sink(SushiLog::LogSink());
    }

    std::vector<CapturedLogLine> LogCapture::lines() const
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        return lines_;
    }
} // namespace SushiDSP