/****************************************************************/
/* LinearPort.hpp                                               */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

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
