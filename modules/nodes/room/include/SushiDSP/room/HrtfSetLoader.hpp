/**************************************************************************/
/* HrtfSetLoader.hpp                                                      */
/**************************************************************************/
/*                         This file is part of:                          */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                        https://sushisystems.io                         */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License");        */
/* you may not use this file except in compliance with the License.       */
/* You may obtain a copy of the License at                                */
/*                                                                        */
/*     http://www.apache.org/licenses/LICENSE-2.0                         */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

/**
 * @file HrtfSetLoader.hpp
 * @brief Reads the flat asset convert_hrtf.py writes into an HrtfSet.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <string>

#include "SushiDSP/room/HrtfSet.hpp"

namespace SushiDSP::room
{
    /// @brief Reads the flat asset convert_hrtf.py writes. Control thread; the only file this
    /// module opens. Spec §9.2 and §9.3.
    class HrtfSetLoader
    {
    public:
        /// @brief Reads path into set.
        /// @param error_out Filled with the reason on failure: missing, bad magic, wrong version,
        /// a rate mismatch, or a truncated file, each naming the path.
        /// @return False and leaves set untouched on any failure.
        static bool load(const std::string& path, double expected_sample_rate, HrtfSet& set,
                         std::string& error_out);
    };
}
