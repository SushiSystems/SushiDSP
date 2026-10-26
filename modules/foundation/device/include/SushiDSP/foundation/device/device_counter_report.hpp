/**************************************************************************/
/* device_counter_report.hpp                                              */
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
 * @file device_counter_report.hpp
 * @brief Declares the control-thread report of a device's counters.
 * @author Mustafa Garip
 * @see modules/foundation/device/README.md
 */

#pragma once

#include <SushiDSP/foundation/core/audio_device.hpp>
#include <SushiDSP/foundation/core/rt_counter_report.hpp>
#include <SushiDSP/foundation/device/device_log_category.hpp>
#include <SushiDSP/foundation/log/log.hpp>

#include <chrono>
#include <string>

namespace SushiDSP
{
    /**
     * @brief Writes what a device's real-time thread counted as one WARN per interval.
     *
     * A host adds its device once it is open, then polls from its control thread. A poll that finds
     * nothing moved writes nothing.
     */
    class DeviceCounterReport final
    {
        public:
            /// @brief Builds a report that speaks at most once per @p interval.
            explicit DeviceCounterReport(std::chrono::seconds interval = std::chrono::seconds(5))
                : reporter_(interval)
            {

            }

            /// @brief Adds the counters of @p device, which must outlive the report.
            void add_device(const AudioDevice& device)
            {
                device.add_counters_to(reporter_);
            }

            /// @brief Polls at @p now and writes a WARN under the device category if one moved.
            void poll(RtCounterReporter::Clock::time_point now)
            {
                const std::string line = reporter_.poll(now);
                if (!line.empty())
                    SD_LOG_WARN(K_DEVICE_LOG_CATEGORY, "{}", line);
            }

            /// @brief Polls at the present time.
            void poll()
            {
                poll(RtCounterReporter::Clock::now());
            }

        private:
            /// @brief Reads the device's counters.
            RtCounterReporter reporter_;
    };
} // namespace SushiDSP
