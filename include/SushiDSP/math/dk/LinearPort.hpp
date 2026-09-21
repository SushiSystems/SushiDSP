/**************************************************************************/
/* LinearPort.hpp                                                         */
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
 * @file LinearPort.hpp
 * @brief The empty nonlinear port a DK block with no nonlinear branches is stepped with.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <array>

namespace SushiDSP::dk
{
    /// @brief The empty nonlinear port a purely linear DK block is stepped with.
    ///
    /// DkCircuit::process_sample takes a model glue by reference whatever its nonlinear branch
    /// count is, and skips the Newton solve entirely when that count is zero. A linear block
    /// passes this.
    struct LinearPort
    {
        /// @brief Does nothing; a block with no nonlinear branches has no currents to evaluate.
        void currents_and_jacobian(const std::array<double, 0>&, std::array<double, 0>&,
                                   std::array<std::array<double, 0>, 0>&) const {}
    };
}
