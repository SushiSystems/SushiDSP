/**************************************************************************/
/* axis_hrtf.hpp                                                          */
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
 * @file axis_hrtf.hpp
 * @brief Builds an HRTF set of six directions, the head the render set edit tests hand to build_render_set.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include <SushiDSP/nodes/room/hrtf_set.hpp>

namespace SushiDSP::test
{
    /// @brief Returns a set of the six axis directions, +x, +y, +z, -x, -y, -z, each with a unit
    /// impulse at tap 0 in both ears and no interaural delay. It stands in for the spherical head,
    /// whose build costs seconds, in a test that reads the geometry of a render set and never the
    /// head's response.
    /// @param sample_rate The rate in hertz the set claims.
    inline room::HrtfSet axis_hrtf(double sample_rate)
    {
        constexpr double K_PI = 3.14159265358979323846;
        const auto half_pi = static_cast<float>(K_PI / 2.0);
        const auto pi = static_cast<float>(K_PI);
        std::vector<room::HrtfSet::Direction> directions = {
            {0.0f, 0.0f, 0.0f}, {half_pi, 0.0f, 0.0f},  {0.0f, half_pi, 0.0f},
            {pi, 0.0f, 0.0f},   {-half_pi, 0.0f, 0.0f}, {0.0f, -half_pi, 0.0f}};
        std::vector<float> left(directions.size() * room::HrtfSet::K_TAP_COUNT, 0.0f);
        for (std::size_t direction = 0; direction < directions.size(); ++direction)
        {
            left[direction * room::HrtfSet::K_TAP_COUNT] = 1.0f;
        }
        std::vector<float> right = left;
        return room::HrtfSet(sample_rate, std::move(directions), std::move(left), std::move(right));
    }
} // namespace SushiDSP::test
