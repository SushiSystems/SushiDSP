/**************************************************************************/
/* default_logger.cpp                                                     */
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
 * @file default_logger.cpp
 * @brief Builds the logger of the process on first use.
 * @author Mustafa Garip
 */

#include <SushiDSP/foundation/log/default_logger.hpp>

#include <memory>

#include <SushiDSP/foundation/log/stderr_log_sink.hpp>

namespace SushiDSP
{
    Logger& default_logger() noexcept
    {
        static Logger logger(std::make_shared<StderrLogSink>(), LogLevel::Info);
        return logger;
    }
} // namespace SushiDSP
