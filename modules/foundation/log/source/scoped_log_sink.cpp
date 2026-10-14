/**************************************************************************/
/* scoped_log_sink.cpp                                                    */
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
 * @file scoped_log_sink.cpp
 * @brief Implements the scoped sink installation.
 * @author Mustafa Garip
 */

#include <SushiDSP/foundation/log/scoped_log_sink.hpp>

#include <utility>

namespace SushiDSP
{
    ScopedLogSink::ScopedLogSink(Logger& logger, std::shared_ptr<LogSink> sink) noexcept
        : logger_(logger), previous_(logger.set_sink(std::move(sink)))
    {
    }

    ScopedLogSink::~ScopedLogSink()
    {
        logger_.flush();
        logger_.set_sink(std::move(previous_));
    }
} // namespace SushiDSP
