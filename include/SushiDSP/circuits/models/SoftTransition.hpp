/**************************************************************************/
/* SoftTransition.hpp                                                     */
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


#pragma once

#include <cmath>

namespace SushiDSP
{
    namespace models
    {
        /// @brief Softplus ln(1 + exp(t)), the smooth C-infinity replacement for max(0, t).
        /// @param t Argument in the units of the caller's transition scale.
        /// @return ln(1 + exp(t)), computed without overflow for large positive t.
        inline double softplus(double t)
        {
            // exp(t) overflows above ~709 and ln(1 + exp(t)) is within 1e-13 of t already at t = 30
            // (ln(1 + exp(30)) - 30 = 9.24e-14), so the linear branch is exact to double precision.
            return t < 30.0 ? std::log1p(std::exp(t)) : t;
        }

        /// @brief Logistic 1 / (1 + exp(-t)), which is the derivative of softplus().
        /// @param t Argument in the units of the caller's transition scale.
        /// @return 1 / (1 + exp(-t)) in [0, 1], computed without overflow for either sign of t.
        inline double logistic(double t)
        {
            // exp(-t) overflows for t below about -709; the equivalent exp(t) / (1 + exp(t)) form
            // underflows gracefully to zero there instead.
            if (t < 0.0)
            {
                const double exponential = std::exp(t);
                return exponential / (1.0 + exponential);
            }

            return 1.0 / (1.0 + std::exp(-t));
        }
    }
}
