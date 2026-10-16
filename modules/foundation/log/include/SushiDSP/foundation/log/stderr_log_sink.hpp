/**************************************************************************/
/* stderr_log_sink.hpp                                                    */
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
 * @file stderr_log_sink.hpp
 * @brief Declares the sink that writes each line to stderr on the calling thread.
 * @author Mustafa Garip
 */

#pragma once

#include <SushiDSP/foundation/log/log_record_sink.hpp>

namespace SushiDSP
{
    /// @brief Writes each record's line, or its message when it has none, to stderr.
    class StderrLogSink final : public LogRecordSink
    {
        public:
            /// @brief Writes the line and its newline with one call, so two threads never interleave.
            void write(const LogRecord& record) noexcept override;

            /// @brief Flushes stderr.
            void flush() noexcept override;
    };
} // namespace SushiDSP
