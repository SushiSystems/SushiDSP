/**************************************************************************/
/* NewtonRaphsonSolver.hpp                                                */
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
 * @file NewtonRaphsonSolver.hpp
 * @brief Defines a Newton-Raphson root finder for a scalar nonlinear equation.
 * @author Mustafa Garip
 */

#pragma once

#include <cmath>
#include <cstdint>
#include <limits>

namespace SushiDSP
{
    /// @brief Generic Newton-Raphson root finder for a scalar nonlinear equation f(x) = 0.
    class NewtonRaphsonSolver
    {
    public:
        /// @brief The residual and its derivative at one trial point, evaluated together.
        ///
        /// Circuit residuals and their derivatives share their expensive term (the same exp()
        /// appears in both), so evaluating them jointly halves the transcendental count per
        /// iteration; the split-callable overload below exists for callers with no such sharing.
        struct Evaluation
        {
            float value;
            float derivative;
        };

        /// @brief Finds x such that the residual is approximately zero, starting from initial_guess.
        /// @tparam JointFunction Any callable Evaluation(float) returning f(x) and f'(x) together; a
        /// template so that no call allocates (include/SushiDSP/README.md).
        /// @param initial_guess Starting point; the previous sample's solution is the intended one.
        /// @param minimum_x Lower bound the iterate is clamped to after every step. Unbounded by
        /// default; an exponential residual needs it finite.
        /// @param maximum_x Upper bound the iterate is clamped to after every step.
        /// @return The converged root estimate. Always finite when the bounds are finite.
        template <typename JointFunction>
        static float solve_joint(
            JointFunction&& function,
            float initial_guess,
            std::uint32_t max_iterations = 4,
            float minimum_x = -std::numeric_limits<float>::infinity(),
            float maximum_x = std::numeric_limits<float>::infinity())
        {
            constexpr float minimum_derivative_magnitude = 1e-6f;

            float x = initial_guess;
            if (!std::isfinite(x))
            {
                x = 0.0f;
            }
            if (x < minimum_x)
            {
                x = minimum_x;
            }
            if (x > maximum_x)
            {
                x = maximum_x;
            }

            for (std::uint32_t iteration = 0; iteration < max_iterations; ++iteration)
            {
                const Evaluation evaluation = function(x);
                float f_prime_value = evaluation.derivative;
                if (std::abs(f_prime_value) < minimum_derivative_magnitude)
                {
                    f_prime_value = (f_prime_value < 0.0f) ? -minimum_derivative_magnitude : minimum_derivative_magnitude;
                }

                float next = x - evaluation.value / f_prime_value;

                // A non-finite step keeps the last good iterate; inf/nan never reaches the warm start.
                if (!std::isfinite(next))
                {
                    break;
                }

                if (next < minimum_x)
                {
                    next = minimum_x;
                }
                if (next > maximum_x)
                {
                    next = maximum_x;
                }
                // A step onto the current iterate is a float fixed point (include/SushiDSP/README.md).
                if (next == x)
                {
                    break;
                }
                x = next;
            }
            return x;
        }

        /// @brief Finds the root from a residual and a derivative supplied as separate callables,
        /// under the contract and arithmetic of solve_joint().
        /// @tparam Residual Any callable float(float).
        /// @tparam Derivative Any callable float(float).
        /// @param minimum_x Lower clamp on the iterate.
        /// @param maximum_x Upper clamp on the iterate.
        /// @return The converged root estimate.
        template <typename Residual, typename Derivative>
        static float solve(
            Residual&& function,
            Derivative&& derivative,
            float initial_guess,
            std::uint32_t max_iterations = 4,
            float minimum_x = -std::numeric_limits<float>::infinity(),
            float maximum_x = std::numeric_limits<float>::infinity())
        {
            return solve_joint(
                [&function, &derivative](float x) { return Evaluation{ function(x), derivative(x) }; },
                initial_guess, max_iterations, minimum_x, maximum_x);
        }
    };
}
