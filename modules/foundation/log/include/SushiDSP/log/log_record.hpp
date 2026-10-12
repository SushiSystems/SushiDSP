/**************************************************************************/
/* log_record.hpp                                                         */
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
 * @file log_record.hpp
 * @brief Declares the one line a logger hands to a sink.
 * @author Mustafa Garip
 */

#pragma once

#include <cstddef>
#include <string_view>

#include "SushiDSP/log/log_category.hpp"
#include "SushiDSP/log/log_level.hpp"

namespace SushiDSP
{
    /// @brief Bounds a formatted message in characters; a longer one is cut at this length.
    inline constexpr std::size_t K_MAX_LOG_MESSAGE_LENGTH = 1024;

    /// @brief Holds one formatted line on its way to a sink.
    struct LogRecord
    {
            LogLevel level;       /**< The severity of the line. */
            LogCategory category; /**< The module the line comes from. */
            std::string_view
                message; /**< The formatted text, valid only during the write() call. */
    };
} // namespace SushiDSP
