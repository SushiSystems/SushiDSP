/**************************************************************************/
/* clap_extensions.hpp                                                    */
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
 * @file clap_extensions.hpp
 * @brief Declares the lookup of the CLAP extension tables the plugin answers.
 * @author Mustafa Garip
 */

#pragma once

namespace SushiDSP::Plugin
{
    /// @brief Finds the table of one CLAP extension: audio-ports, state, latency or params.
    /// @param id The extension's CLAP id.
    /// @return The extension's callback table, or nullptr for an extension the plugin lacks.
    [[nodiscard]] const void* find_extension(const char* id) noexcept;
} // namespace SushiDSP::Plugin
