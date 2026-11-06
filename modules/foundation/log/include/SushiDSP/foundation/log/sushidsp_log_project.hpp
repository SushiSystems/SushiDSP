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
 * @file sushidsp_log_project.hpp
 * @brief Declares the SushiLog project every SushiDSP category belongs to.
 * @author Mustafa Garip
 */

#pragma once

#include <SushiLog/log/log_project.hpp>

namespace SushiDSP
{
    /// @brief Holds the SushiDSP project, whose levels SD_LOG_LEVEL sets.
    inline constexpr SushiLog::LogProject K_SUSHIDSP_LOG_PROJECT{"sushidsp", "SD_LOG_LEVEL"};
} // namespace SushiDSP