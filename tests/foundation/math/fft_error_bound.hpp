/**************************************************************************/
/* fft_error_bound.hpp                                                    */
/**************************************************************************/
/*                          This file is part of:                         */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                         https://sushisystems.io                        */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License"); you    */
/* may not use this file except in compliance with the License. You may   */
/* obtain a copy of the License at                                        */
/*                                                                        */
/*     https://www.apache.org/licenses/LICENSE-2.0                        */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

/**
 * @file fft_error_bound.hpp
 * @brief Derives the rounding bounds of the float radix-2 FFT and of the partitioned convolution built
 * on it, for the tests of both.
 * @author Mustafa Garip
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "support/tolerance.hpp"

namespace TestSupport
{

/**
 * @brief Returns the relative 2-norm bound of a float radix-2 FFT of the given size.
 *
 * log2(N) eta / (1 - log2(N) eta) with eta = mu + gamma4 (sqrt 2 + mu) and mu = u for twiddles rounded
 * to float (Higham, Accuracy and Stability of Numerical Algorithms, theorem 24.2). The bound is zero at
 * size 1, where the transform is a copy.
 * @param size The transform size, a power of two.
 */
[[nodiscard]] inline double fft_relative_bound(std::uint32_t size) noexcept
{
    double const u = unit_roundoff<float>();
    double const stages = std::log2(static_cast<double>(size));
    double const gamma4 = 4.0 * u / (1.0 - 4.0 * u);
    double const eta = u + gamma4 * (std::sqrt(2.0) + u);
    return stages * eta / (1.0 - stages * eta);
}

/**
 * @brief Returns the largest l1 norm of a partition-sized slice of an impulse response.
 * @param response The impulse response.
 * @param partition The slice length.
 */
[[nodiscard]] inline double largest_slice_l1(std::vector<float> const& response, std::size_t partition)
{
    double largest = 0.0;
    for (std::size_t begin = 0; begin < response.size(); begin += partition)
    {
        double sum = 0.0;
        for (std::size_t i = begin; i < std::min(response.size(), begin + partition); ++i)
        {
            sum += std::abs(static_cast<double>(response[i]));
        }
        largest = std::max(largest, sum);
    }
    return largest;
}

/**
 * @brief Returns the largest 2-norm of a partition-sized block of a signal.
 * @param signal The signal.
 * @param partition The block length.
 */
[[nodiscard]] inline double largest_block_norm(std::vector<float> const& signal, std::size_t partition)
{
    double largest = 0.0;
    for (std::size_t begin = 0; begin < signal.size(); begin += partition)
    {
        double sum = 0.0;
        for (std::size_t i = begin; i < std::min(signal.size(), begin + partition); ++i)
        {
            sum += static_cast<double>(signal[i]) * signal[i];
        }
        largest = std::max(largest, std::sqrt(sum));
    }
    return largest;
}

/**
 * @brief Returns the 2-norm error bound of one output block of a partitioned convolution, derived in
 * tests/foundation/math/README.md.
 * @param partition The partition size N; the transform size is 2N.
 * @param partitions The number of partitions P.
 * @param block_norm The largest 2-norm of an input block.
 * @param slice_l1 The largest l1 norm of an impulse response slice.
 */
[[nodiscard]] inline double partitioned_block_error_bound(std::uint32_t partition, std::size_t partitions,
                                                          double block_norm, double slice_l1)
{
    double const factor =
        3.0 * fft_relative_bound(2 * partition) + (static_cast<double>(partitions) + 2.0) * unit_roundoff<float>();
    return factor * static_cast<double>(partitions) * block_norm * slice_l1;
}

} // namespace TestSupport
