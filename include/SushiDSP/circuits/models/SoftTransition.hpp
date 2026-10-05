/****************************************************************/
/* SoftTransition.hpp                                           */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

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
    }
}
