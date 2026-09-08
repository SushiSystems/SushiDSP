/**************************************************************************/
/* CrossoverPair.cpp                                                      */
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

/**
 * @file CrossoverPair.cpp
 * @brief Two Butterworth pairs per half, cascaded into a Linkwitz-Riley split.
 * @author Mustafa Garip & Sushi Systems
 */

#include "SushiDSP/room/CrossoverPair.hpp"

namespace SushiDSP::room
{
    namespace
    {
        /// @brief Holds the Butterworth quality factor 1 / sqrt(2), whose squared magnitude pair
        /// sums to one at every frequency.
        constexpr double kButterworthQ = 0.70710678118654752440;
    }

    void CrossoverPair::set_crossover(double sample_rate, double crossover_hz) noexcept
    {
        bypassed_ = crossover_hz <= 0.0;
        if (bypassed_)
        {
            reset();
            return;
        }
        low_first_.set_low_pass(sample_rate, crossover_hz, kButterworthQ);
        low_second_.set_low_pass(sample_rate, crossover_hz, kButterworthQ);
        high_first_.set_high_pass(sample_rate, crossover_hz, kButterworthQ);
        high_second_.set_high_pass(sample_rate, crossover_hz, kButterworthQ);
    }

    void CrossoverPair::process_sample(float input, float& low_out, float& high_out) noexcept
    {
        if (bypassed_)
        {
            low_out = 0.0f;
            high_out = input;
            return;
        }
        // At fourth order the two halves are in phase at every frequency and add directly, with
        // no inversion of either half.
        low_out = low_second_.process_sample(low_first_.process_sample(input));
        high_out = high_second_.process_sample(high_first_.process_sample(input));
    }

    void CrossoverPair::reset() noexcept
    {
        low_first_.reset();
        low_second_.reset();
        high_first_.reset();
        high_second_.reset();
    }
}
