/**************************************************************************/
/* log_delivery.hpp                                                       */
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
 * @file log_delivery.hpp
 * @brief Declares the two ways a logger hands its records to the sink.
 * @author Mustafa Garip
 */

#pragma once

#include <cstdint>

namespace SushiDSP
{
    /// @brief Names the thread a record reaches the sink on.
    enum class LogDelivery : std::uint8_t
    {
        Asynchronous, /**< A logger thread delivers each record from a bounded queue. */
        Synchronous,  /**< The calling thread delivers each record before the call returns. */
    };
} // namespace SushiDSP
