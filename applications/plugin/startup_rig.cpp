/**************************************************************************/
/* startup_rig.cpp                                                        */
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
 * @file startup_rig.cpp
 * @brief Implements the reader of the rig preset a plugin starts on when it has no state.
 * @author Mustafa Garip
 */

#include "startup_rig.hpp"

#include <cstdlib>
#include <fstream>
#include <sstream>

namespace SushiDSP::Plugin
{
    std::optional<std::string> read_startup_rig()
    {
        const char* path = std::getenv(K_STARTUP_RIG_VARIABLE);
        if (path == nullptr || path[0] == '\0')
        {
            return std::nullopt;
        }
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open())
        {
            return std::nullopt;
        }
        std::ostringstream text;
        text << file.rdbuf();
        return text.str();
    }
} // namespace SushiDSP::Plugin
