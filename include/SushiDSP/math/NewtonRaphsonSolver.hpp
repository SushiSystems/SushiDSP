/**************************************************************************/
/* NewtonRaphsonSolver.hpp                                                */
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
#include <cstdint>
#include <limits>

namespace SushiDSP
{
    /// @brief Generic Newton-Raphson root finder for a scalar nonlinear equation f(x) = 0.
    class NewtonRaphsonSolver
    {
    public:
        /// @brief Finds x such that function(x) is approximately zero, starting from initial_guess.
        /// @tparam Residual Any callable float(float). Templated rather than type-erased through
        /// std::function on purpose: every caller sits on the audio thread and builds a fresh
        /// capturing lambda per sample, and a capture list wider than the standard library's
        /// small-buffer optimisation (16 bytes on libstdc++) would make std::function heap-allocate
        /// inside process(). A template instantiates the callable inline, with no allocation on any
        /// platform.
        /// @tparam Derivative Any callable float(float).
        /// @param function The residual function f(x).
        /// @param derivative The derivative f'(x).
        /// @param initial_guess Starting point; warm-starting from the previous sample's
        /// solution is what makes 2-4 iterations sufficient at audio rates.
        /// @param max_iterations Maximum Newton iterations.
        /// @param minimum_x Lower bound the iterate is clamped to after every step. Circuits with
        /// exponential residuals (diode models) must bound the trial point, otherwise a single
        /// overshoot evaluates exp() past the float range and poisons the warm-start state with
        /// inf/nan forever. Defaults to unbounded.
        /// @param maximum_x Upper bound the iterate is clamped to after every step.
        /// @return The converged root estimate. Always finite when the bounds are finite.
        template <typename Residual, typename Derivative>
        static float solve(
            Residual&& function,
            Derivative&& derivative,
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
                const float f_value = function(x);
                float f_prime_value = derivative(x);

                if (std::abs(f_prime_value) < minimum_derivative_magnitude)
                {
                    f_prime_value = (f_prime_value < 0.0f) ? -minimum_derivative_magnitude : minimum_derivative_magnitude;
                }

                float next = x - f_value / f_prime_value;

                // A non-finite step means the residual blew past the float range; keep the last
                // good iterate rather than propagating inf/nan into the caller's warm-start state.
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

                x = next;
            }

            return x;
        }
    };
}
