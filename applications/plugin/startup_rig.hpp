/**************************************************************************/
/* startup_rig.hpp                                                        */
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
 * @file startup_rig.hpp
 * @brief Declares the reader of the rig preset a plugin starts on when it has no state.
 * @author Mustafa Garip
 */

#pragma once

#include <optional>
#include <string>

namespace SushiDSP::Plugin
{
    /// @brief Names the environment variable that holds the path of the startup rig preset.
    constexpr const char* K_STARTUP_RIG_VARIABLE = "SUSHIDSP_PLUGIN_RIG";

    /// @brief Reads the rig preset K_STARTUP_RIG_VARIABLE names. Control thread only.
    /// @return The file's text, or std::nullopt when the variable is unset or empty, or the file
    /// will not open.
    [[nodiscard]] std::optional<std::string> read_startup_rig();
} // namespace SushiDSP::Plugin
