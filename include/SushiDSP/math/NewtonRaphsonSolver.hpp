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

#include <cstdint>
#include <functional>
#include <limits>

namespace SushiDSP
{
    /// @brief Generic Newton-Raphson root finder for a scalar nonlinear equation f(x) = 0.
    class NewtonRaphsonSolver
    {
    public:
        /// @brief Finds x such that function(x) is approximately zero, starting from initial_guess.
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
        static float solve(
            const std::function<float(float)>& function,
            const std::function<float(float)>& derivative,
            float initial_guess,
            std::uint32_t max_iterations = 4,
            float minimum_x = -std::numeric_limits<float>::infinity(),
            float maximum_x = std::numeric_limits<float>::infinity());
    };
}
