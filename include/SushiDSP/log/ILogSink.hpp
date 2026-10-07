/**************************************************************************/
/* ILogSink.hpp                                                           */
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
 * @file ILogSink.hpp
 * @brief Declares the interface a logger writes its lines through.
 * @author Mustafa Garip
 */

#pragma once

#include "SushiDSP/log/LogRecord.hpp"

namespace SushiDSP
{
    /// @brief Receives formatted log lines and puts them somewhere.
    class ILogSink
    {
    public:
        virtual ~ILogSink() = default;

        /// @brief Takes one line. Any thread may call it, several at once.
        /// @param record Its message is valid only during the call; a sink that keeps it copies it.
        virtual void write(const LogRecord& record) noexcept = 0;

        /// @brief Returns once every line written before the call has reached its destination.
        virtual void flush() noexcept = 0;
    };
}
