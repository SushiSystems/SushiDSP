/**************************************************************************/
/* tubescreamer_log_category.hpp                                          */
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
 * @file tubescreamer_log_category.hpp
 * @brief Declares the category the tubescreamer module logs under.
 * @author Mustafa Garip
 */

#pragma once

#include <SushiLog/log/log_category.hpp>

#include <SushiDSP/foundation/log/sushidsp_log_project.hpp>

/// @brief Names the lines the tubescreamer module writes.
inline constexpr SushiLog::LogCategory K_TUBESCREAMER_LOG_CATEGORY =
    SushiLog::make_log_category(SushiDSP::K_SUSHIDSP_LOG_PROJECT, "tubescreamer");
