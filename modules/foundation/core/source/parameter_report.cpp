/**************************************************************************/
/* parameter_report.cpp                                                   */
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
 * @file parameter_report.cpp
 * @brief Defines the record a by-id parameter write writes when no row carries the id.
 * @author Mustafa Garip
 */

#include <SushiDSP/foundation/core/parameter_report.hpp>

#include <SushiDSP/foundation/core/core_log_category.hpp>
#include <SushiDSP/foundation/log/log.hpp>

namespace SushiDSP
{
    void report_unknown_parameter(const char* id, float value) noexcept
    {
        SD_LOG_WARN(K_CORE_LOG_CATEGORY,
                    "write of {} to unknown parameter id '{}' ignored; the node keeps its value",
                    value, id);
    }
} // namespace SushiDSP
