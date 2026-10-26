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
