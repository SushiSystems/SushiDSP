/**************************************************************************/
/* log_site.hpp                                                           */
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
 * @file log_site.hpp
 * @brief Declares the file and line a log call was made at.
 * @author Mustafa Garip
 */

#pragma once

#include <cstddef>
#include <string_view>

namespace SushiDSP
{
    /// @brief Holds the source position of one log call.
    struct LogSite
    {
            const char* file; /**< The path __FILE__ gives; never null, static storage. */
            int line;         /**< The line __LINE__ gives. */
    };

    /// @brief Returns the part of a path after its last slash or backslash.
    constexpr std::string_view log_site_file_name(const char* path) noexcept
    {
        const std::string_view whole(path);
        const std::size_t slash = whole.find_last_of("/\\");
        return slash == std::string_view::npos ? whole : whole.substr(slash + 1);
    }
} // namespace SushiDSP
