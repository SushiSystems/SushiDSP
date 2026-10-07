/**************************************************************************/
/* StderrLogSink.hpp                                                      */
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
 * @file StderrLogSink.hpp
 * @brief Declares the sink that writes each line to stderr on the calling thread.
 * @author Mustafa Garip
 */

#pragma once

#include "SushiDSP/log/ILogSink.hpp"

namespace SushiDSP
{
    /// @brief Writes `[LEVEL] [category] message` to stderr before write() returns.
    class StderrLogSink final : public ILogSink
    {
    public:
        /// @brief Writes one line with a single call, so lines of two threads never interleave.
        void write(const LogRecord& record) noexcept override;

        /// @brief Flushes stderr.
        void flush() noexcept override;
    };
}
