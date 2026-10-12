/**************************************************************************/
/* default_logger.hpp                                                     */
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
 * @file default_logger.hpp
 * @brief Declares the one logger of the process, the one the SD_LOG macros write through.
 * @author Mustafa Garip
 */

#pragma once

#include "SushiDSP/log/logger.hpp"

namespace SushiDSP
{
    /// @brief Returns the logger of the process.
    /// It starts with a StderrLogSink and LogLevel::Info, so a line logged before an application
    /// installs its own sink still reaches stderr.
    Logger& default_logger() noexcept;
} // namespace SushiDSP
