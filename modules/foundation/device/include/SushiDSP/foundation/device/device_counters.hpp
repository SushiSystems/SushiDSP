/**************************************************************************/
/* device_counters.hpp                                                    */
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
 * @file device_counters.hpp
 * @brief Declares the counters a device raises on its real-time thread.
 * @author Mustafa Garip
 * @see modules/foundation/device/README.md
 */

#pragma once

#include <SushiDSP/foundation/core/rt_counter.hpp>
#include <SushiDSP/foundation/core/rt_counter_report.hpp>

namespace SushiDSP
{
    /**
     * @brief Holds what a device counts instead of logging it from its callback.
     *
     * A host adds them to an RtCounterReporter and logs the report on its control thread.
     */
    struct DeviceCounters
    {
            /// @brief Blocks answered with silence because no callback was installed.
            RtCounter no_callback;

            /// @brief Blocks longer than the device's scratch, answered with silence; the peak is
            /// the largest frame count asked.
            RtCounter oversize_block;

            /// @brief Blocks that arrived more than one and a half block periods after the last.
            RtCounter arrival_gap;

            /// @brief Blocks whose processing passed their own block period.
            RtCounter overrun;

            /// @brief Overload, resync, latency-change and reset messages the driver sent.
            RtCounter driver_message;

            /// @brief Driver calls made from the callback that returned an error.
            RtCounter driver_error;

            /// @brief Adds every counter to @p reporter under its name; they must outlive it.
            void add_to(RtCounterReporter& reporter) const
            {
                reporter.add("no_callback", no_callback);
                reporter.add("oversize_block", oversize_block);
                reporter.add("arrival_gap", arrival_gap);
                reporter.add("overrun", overrun);
                reporter.add("driver_message", driver_message);
                reporter.add("driver_error", driver_error);
            }
    };
} // namespace SushiDSP
