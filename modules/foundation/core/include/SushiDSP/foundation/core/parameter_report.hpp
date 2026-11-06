/**************************************************************************/
/* parameter_report.hpp                                                   */
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
 * @file parameter_report.hpp
 * @brief Declares the record a by-id parameter write writes when no row carries the id.
 * @author Mustafa Garip
 */

#pragma once

namespace SushiDSP
{
    /// @brief Writes one WARN under the core category naming @p id and @p value, which a table
    /// ignored. Control thread only; defined once so a parameter table includes no log header.
    void report_unknown_parameter(const char* id, float value) noexcept;
} // namespace SushiDSP
