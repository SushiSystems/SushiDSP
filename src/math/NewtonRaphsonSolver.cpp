/**************************************************************************/
/* NewtonRaphsonSolver.cpp                                                */
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

#include "SushiDSP/math/NewtonRaphsonSolver.hpp"

#include <cmath>

namespace SushiDSP
{
    float NewtonRaphsonSolver::solve(
        const std::function<float(float)>& function,
        const std::function<float(float)>& derivative,
        float initial_guess,
        std::uint32_t max_iterations,
        float minimum_x,
        float maximum_x)
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
}
