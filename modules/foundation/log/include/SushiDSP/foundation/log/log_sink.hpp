/**************************************************************************/
/* log_sink.hpp                                                           */
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
 * @file log_sink.hpp
 * @brief Declares the callable that receives each record of the process logger.
 * @author Mustafa Garip
 */

#pragma once

#include <SushiDSP/foundation/log/delegate.hpp>
#include <SushiDSP/foundation/log/log_record.hpp>

namespace SushiDSP
{
    /// @brief Receives each record; an empty sink means stderr.
    /// A sink never logs, and never sets the sink, a level or the delivery.
    using LogSink = Delegate<void(const LogRecord&), 48>;
} // namespace SushiDSP
