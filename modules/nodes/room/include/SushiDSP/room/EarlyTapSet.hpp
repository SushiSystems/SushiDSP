/**************************************************************************/
/* EarlyTapSet.hpp                                                        */
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
 * @file EarlyTapSet.hpp
 * @brief The image-source taps the control thread publishes to the early renderer.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace SushiDSP::room
{
    /// @brief Holds one image source, reduced to what the audio thread reads. Spec §5.2.
    struct EarlyTap
    {
        /// @brief Stores the fractional delay in frames, read back from the ring's write head.
        float delay_frames;

        /// @brief Stores 1/r times the specular share times the level, linear.
        float gain;

        /// @brief Stores b0, b1, b2, a1, a2 of the fitted filter's first section.
        std::array<double, 5> section_a;

        /// @brief Stores b0, b1, b2, a1, a2 of the fitted filter's second section.
        std::array<double, 5> section_b;

        /// @brief Stores the ACN/SN3D order-3 encode gains, or the pattern gain then zeros.
        std::array<float, 16> encode;
    };

    /// @brief Holds every early tap of one scene and the seam where the tail takes over.
    struct EarlyTapSet
    {
        /// @brief Bounds the taps one set carries.
        static constexpr std::size_t kMaxEarlyTaps = 128;

        /// @brief Stores the taps; entries past count are never read.
        std::array<EarlyTap, kMaxEarlyTaps> taps;

        /// @brief Counts the taps in use.
        std::uint32_t count;

        /// @brief Stores t_c in seconds, where the tail takes over.
        float crossover_time_s;

        /// @brief Stores the energy handed to the tail's input, linear.
        float scattered_energy;
    };
}
