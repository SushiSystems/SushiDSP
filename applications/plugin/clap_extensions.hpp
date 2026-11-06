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
/* Licensed under the Apache License, Version 2.0 (the "License"); you    */
/* may not use this file except in compliance with the License. You may   */
/* obtain a copy of the License at                                        */
/*                                                                        */
/*     https://www.apache.org/licenses/LICENSE-2.0                        */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
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
