/**************************************************************************/
/* ScopedLogSink.hpp                                                      */
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
 * @file ScopedLogSink.hpp
 * @brief Declares the guard that installs a sink in a logger for the length of a scope.
 * @author Mustafa Garip
 */

#pragma once

#include <memory>

#include "SushiDSP/log/Logger.hpp"

namespace SushiDSP
{
    /// @brief Installs a sink on construction; on destruction flushes it and puts the old one back.
    class ScopedLogSink final
    {
    public:
        /// @brief Installs the sink.
        /// @param logger Must outlive the guard.
        /// @param sink Must not be null.
        ScopedLogSink(Logger& logger, std::shared_ptr<ILogSink> sink) noexcept;

        /// @brief Flushes the logger and restores the sink that was in place before.
        ~ScopedLogSink();

        ScopedLogSink(const ScopedLogSink&) = delete;
        ScopedLogSink& operator=(const ScopedLogSink&) = delete;
        ScopedLogSink(ScopedLogSink&&) = delete;
        ScopedLogSink& operator=(ScopedLogSink&&) = delete;

    private:
        /// @brief Refers to the logger the sink was installed in.
        Logger& logger_;

        /// @brief Holds the sink to restore.
        std::shared_ptr<ILogSink> previous_;
    };
}
