/**************************************************************************/
/* sushidsp_log_project.hpp                                               */
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
 * @file sushidsp_log_project.hpp
 * @brief Declares the log project every SushiDSP category belongs to.
 * @author Mustafa Garip
 */

#pragma once

#include <SushiDSP/foundation/log/log_category.hpp>
#include <SushiDSP/foundation/log/log_project.hpp>

namespace SushiDSP
{
    /// @brief Holds the SushiDSP project, whose levels SD_LOG_LEVEL sets.
    inline constexpr LogProject K_SUSHIDSP_LOG_PROJECT{"sushidsp", "SD_LOG_LEVEL"};
} // namespace SushiDSP
