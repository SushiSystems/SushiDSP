/**************************************************************************/
/* log_log_category.hpp                                                   */
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
 * @file log_log_category.hpp
 * @brief Declares the category the logging bricks report their own failures under.
 * @author Mustafa Garip
 */

#pragma once

#include "SushiDSP/log/LogCategory.hpp"

/// @brief Names the lines the log folder writes about itself, such as dropped lines.
inline constexpr SushiDSP::LogCategory K_LOG_LOG_CATEGORY = SushiDSP::make_log_category("log");
