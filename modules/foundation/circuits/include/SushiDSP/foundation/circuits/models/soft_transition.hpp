/**************************************************************************/
/* soft_transition.hpp                                                    */
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
 * @file soft_transition.hpp
 * @brief Defines the softplus and logistic functions the device models share.
 * @author Mustafa Garip
 */

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
            // From t = 30 up softplus equals t to double precision (modules/foundation/circuits/README.md).
            if (t >= 30.0)
            {
                softplus_value = t;
                logistic_value = 1.0 / (1.0 + std::exp(-t));
                return;
            }

            // exp(t) underflows to zero for very negative t, where exp(-t) would overflow to inf.
            const double exponential = std::exp(t);
            softplus_value = std::log1p(exponential);
            logistic_value = exponential / (1.0 + exponential);
        }

        /// @brief Softplus ln(1 + exp(t)), the smooth C-infinity replacement for max(0, t).
        /// @param t Argument in the units of the caller's transition scale.
        /// @return ln(1 + exp(t)), computed without overflow for large positive t.
        inline double softplus(double t)
        {
            if (t >= 30.0)
            {
                return t;
            }
            return std::log1p(std::exp(t));
        }

        /// @brief Logistic 1 / (1 + exp(-t)), which is the derivative of softplus().
        /// @param t Argument in the units of the caller's transition scale.
        /// @return 1 / (1 + exp(-t)) in [0, 1], computed without overflow for either sign of t.
        inline double logistic(double t)
        {
            if (t >= 30.0)
            {
                return 1.0 / (1.0 + std::exp(-t));
            }
            const double exponential = std::exp(t);
            return exponential / (1.0 + exponential);
        }
    } // namespace models
} // namespace SushiDSP
