/**************************************************************************/
/* rt_counter_report.cpp                                                  */
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
 * @file rt_counter_report.cpp
 * @brief Defines the reporter of the real-time counters.
 * @author Mustafa Garip
 */

#include <SushiDSP/foundation/core/rt_counter_report.hpp>

#include <cmath>
#include <utility>

namespace SushiDSP
{
    RtCounterReporter::RtCounterReporter(Clock::duration interval) : interval_(interval)
    {

    }

    void RtCounterReporter::add(const char* label, const RtCounter& counter)
    {
        entries_.push_back(
            Entry{label, [&counter] { return counter.value(); }, &counter, counter.value()});
    }

    void RtCounterReporter::add(const char* label, std::function<std::uint64_t()> read)
    {
        const std::uint64_t now = read();
        entries_.push_back(Entry{label, std::move(read), nullptr, now});
    }

    std::string RtCounterReporter::poll(Clock::time_point now)
    {
        if (!started_)
        {
            started_ = true;
            last_report_ = now;
            return std::string();
        }
        if (now - last_report_ < interval_)
            return std::string();

        const double seconds = std::chrono::duration<double>(now - last_report_).count();
        last_report_ = now;

        std::string line;
        for (Entry& entry : entries_)
        {
            const std::uint64_t value = entry.read();
            if (value == entry.reported)
                continue;
            if (!line.empty())
                line += ", ";
            line += std::to_string(value - entry.reported) + " " + entry.label;
            entry.reported = value;
            if (entry.counter != nullptr && entry.counter->peak() != 0U)
                line += " (largest " + std::to_string(entry.counter->peak()) + ")";
        }
        if (!line.empty())
            line += " in " + std::to_string(static_cast<std::uint64_t>(std::llround(seconds))) +
                    " s";
        return line;
    }
} // namespace SushiDSP
