/**************************************************************************/
/* capture_log.hpp                                                        */
/**************************************************************************/
/*                          This file is part of:                         */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                         https://sushisystems.io                        */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License"); you    */
/* may not use this file except in compliance with the License. You may   */
/* obtain a copy of the License at                                        */
/*                                                                        */
/*     https://www.apache.org/licenses/LICENSE-2.0                        */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

/**
 * @file capture_log.hpp
 * @brief Declares the port through which TrackRecorder reports a failed take.
 * @author Mustafa Garip
 */

#pragma once

#include <cstdint>
#include <string>

namespace SushiDSP
{
    /// @brief The failure TrackRecorder reports.
    enum class CaptureFailureCause : std::uint8_t
    {
        TakeFileNotOpened,      /**< arm() could not open the take file. */
        WriterThreadNotStarted, /**< arm() could not start the writer thread. */
        AppendFailed,           /**< The writer thread could not append frames to the take file. */
    };

    /// @brief What TrackRecorder reports about one failure.
    struct CaptureFailure
    {
        CaptureFailureCause cause;     /**< What failed. */
        std::string path;              /**< The take path; empty for AppendFailed. */
        std::string detail;            /**< The exception's message, or a note that its type was unknown. */
        std::uint64_t frames;          /**< The frames of the failed append; 0 for the other causes. */
        std::uint64_t frames_written;  /**< The frames already in the file; 0 for the other causes. */
    };

    /// @brief Receives each failure TrackRecorder meets. The product's adapter is
    /// SushiLogCaptureLog; InMemoryCaptureLog keeps the failures for a test. It is called on the
    /// control thread and on the writer thread, never on the audio thread, so an adapter may
    /// allocate and lock.
    class CaptureLog
    {
        public:
            CaptureLog() = default;
            CaptureLog(const CaptureLog&) = delete;
            CaptureLog& operator=(const CaptureLog&) = delete;
            CaptureLog(CaptureLog&&) = delete;
            CaptureLog& operator=(CaptureLog&&) = delete;
            virtual ~CaptureLog() = default;

            /// @brief Takes @p failure, met on the control thread or the writer thread.
            virtual void report_failure(const CaptureFailure& failure) = 0;
    };
} // namespace SushiDSP
