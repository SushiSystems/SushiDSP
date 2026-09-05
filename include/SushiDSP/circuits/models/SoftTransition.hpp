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
        /// @brief Evaluates softplus and its derivative together from one exponential.
        ///
        /// softplus() and logistic() each cost one exp(); a caller that needs both — which every
        /// device model's joint current-and-Jacobian entry point does — would otherwise pay for
        /// two. The branches are the same overflow guards the two single-value helpers carried.
        /// @param t Argument in the units of the caller's transition scale.
        /// @param softplus_value Receives ln(1 + exp(t)).
        /// @param logistic_value Receives 1 / (1 + exp(-t)).
        inline void softplus_and_logistic(double t, double& softplus_value, double& logistic_value)
        {
            // ln(1 + exp(t)) - t is 9.24e-14 at t = 30 and falls from there, so the linear branch
            // is exact to double precision and keeps exp(t) away from its ~709 overflow.
            if (t >= 30.0)
            {
                softplus_value = t;
                logistic_value = 1.0 / (1.0 + std::exp(-t));
                return;
            }

            // exp(t) underflows gracefully to zero for very negative t, giving softplus 0 and
            // logistic 0 rather than the inf that exp(-t) would produce.
            const double exponential = std::exp(t);
            softplus_value = std::log1p(exponential);
            logistic_value = exponential / (1.0 + exponential);
        }

        /// @brief Softplus ln(1 + exp(t)), the smooth C-infinity replacement for max(0, t).
        /// @param t Argument in the units of the caller's transition scale.
        /// @return ln(1 + exp(t)), computed without overflow for large positive t.
        inline double softplus(double t)
        {
            double softplus_value = 0.0;
            double logistic_value = 0.0;
            softplus_and_logistic(t, softplus_value, logistic_value);
            return softplus_value;
        }

        /// @brief Logistic 1 / (1 + exp(-t)), which is the derivative of softplus().
        /// @param t Argument in the units of the caller's transition scale.
        /// @return 1 / (1 + exp(-t)) in [0, 1], computed without overflow for either sign of t.
        inline double logistic(double t)
        {
            double softplus_value = 0.0;
            double logistic_value = 0.0;
            softplus_and_logistic(t, softplus_value, logistic_value);
            return logistic_value;
        }
    }
}
