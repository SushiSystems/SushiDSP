/**************************************************************************/
/* SincResampler.cpp                                                     */
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

#include "SushiDSP/math/SincResampler.hpp"

#include <cmath>
#include <cstddef>
#include <stdexcept>

namespace SushiDSP
{
    namespace
    {
        constexpr int    HALF_TAP_COUNT = 32;
        constexpr double KAISER_BETA    = 8.6;
        constexpr double PI             = 3.14159265358979323846;

        double bessel_i0(double x)
        {
            double sum  = 1.0;
            double term = 1.0;
            for (int k = 1; k < 40; ++k)
            {
                term *= (x / (2.0 * k)) * (x / (2.0 * k));
                sum += term;
                if (term < sum * 1e-17)
                {
                    break;
                }
            }
            return sum;
        }

        double normalised_sinc(double x)
        {
            if (std::abs(x) < 1e-12)
            {
                return 1.0;
            }
            const double pi_x = PI * x;
            return std::sin(pi_x) / pi_x;
        }
    }

    std::vector<float> SincResampler::resample(const std::vector<float>& input, double input_rate, double output_rate)
    {
        if (input_rate <= 0.0 || output_rate <= 0.0)
        {
            throw std::invalid_argument("SincResampler::resample: sample rates must be positive");
        }

        if (input.empty())
        {
            return {};
        }

        const double ratio = input_rate / output_rate;
        if (std::abs(ratio - 1.0) < 1e-12)
        {
            return input;
        }

        const double cutoff              = ratio < 1.0 ? 1.0 : 1.0 / ratio;
        const double inverse_beta_scale  = 1.0 / bessel_i0(KAISER_BETA);
        const auto   output_size         = static_cast<std::size_t>(
            std::llround(static_cast<double>(input.size()) / ratio));

        std::vector<float> output(output_size, 0.0f);
        const auto last_index = static_cast<long long>(input.size()) - 1;

        for (std::size_t n = 0; n < output_size; ++n)
        {
            const double    position = static_cast<double>(n) * ratio;
            const long long centre   = static_cast<long long>(std::floor(position));

            double accumulator = 0.0;
            double weight_sum  = 0.0;

            for (long long k = centre - HALF_TAP_COUNT; k <= centre + HALF_TAP_COUNT; ++k)
            {
                if (k < 0 || k > last_index)
                {
                    continue;
                }

                const double offset = position - static_cast<double>(k);
                const double ratio_in_window = offset / static_cast<double>(HALF_TAP_COUNT);
                if (std::abs(ratio_in_window) > 1.0)
                {
                    continue;
                }

                const double window = bessel_i0(KAISER_BETA * std::sqrt(1.0 - ratio_in_window * ratio_in_window))
                                      * inverse_beta_scale;
                const double tap = normalised_sinc(cutoff * offset) * window;

                accumulator += static_cast<double>(input[static_cast<std::size_t>(k)]) * tap;
                weight_sum += tap;
            }

            output[n] = weight_sum != 0.0 ? static_cast<float>(accumulator / weight_sum) : 0.0f;
        }

        return output;
    }
}
