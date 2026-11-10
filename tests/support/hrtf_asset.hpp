/**************************************************************************/
/* hrtf_asset.hpp                                                         */
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
 * @file hrtf_asset.hpp
 * @brief Builds the bytes of an HRTF asset of six directions, the file convert_hrtf.py writes.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include <SushiDSP/nodes/room/hrtf_set.hpp>

namespace SushiDSP::test
{
    /// @brief Returns the bytes of an asset that HrtfSetLoader::decode reads into the six-direction
    /// set of axis_hrtf.hpp, with a zero decoder of order 1 and a zero diffuse field. It stands in
    /// for the file of a test that reads which set a rig hands on, never the set's response.
    /// @param sample_rate_hz The rate the asset claims.
    inline std::vector<unsigned char> axis_hrtf_asset(std::uint32_t sample_rate_hz)
    {
        constexpr std::uint32_t K_DIRECTIONS = 6;
        constexpr std::uint32_t K_ORDER = 1;
        constexpr std::uint32_t K_CHANNELS = (K_ORDER + 1) * (K_ORDER + 1);
        constexpr float K_HALF_PI = 1.57079632679489661923f;
        constexpr float K_PI = 3.14159265358979323846f;
        const float angles[K_DIRECTIONS][2] = {{0.0f, 0.0f},       {K_HALF_PI, 0.0f},
                                               {0.0f, K_HALF_PI},  {K_PI, 0.0f},
                                               {-K_HALF_PI, 0.0f}, {0.0f, -K_HALF_PI}};
        std::vector<unsigned char> bytes;
        const auto put_u32 = [&bytes](std::uint32_t value)
        {
            for (int shift = 0; shift < 32; shift += 8)
            {
                bytes.push_back(static_cast<unsigned char>((value >> shift) & 0xffu));
            }
        };
        const auto put_f32 = [&put_u32](float value)
        {
            std::uint32_t raw = 0;
            std::memcpy(&raw, &value, sizeof(raw));
            put_u32(raw);
        };
        const unsigned char magic[8] = {'S', 'D', 'H', 'R', 'T', 'F', 0, 0};
        bytes.insert(bytes.end(), magic, magic + sizeof(magic));
        put_u32(1);
        put_u32(sample_rate_hz);
        put_u32(K_DIRECTIONS);
        put_u32(room::HrtfSet::K_TAP_COUNT);
        put_u32(K_ORDER);
        put_u32(0);
        for (std::uint32_t direction = 0; direction < K_DIRECTIONS; ++direction)
        {
            put_f32(angles[direction][0]);
            put_f32(angles[direction][1]);
            put_f32(0.0f);
            for (int ear = 0; ear < 2; ++ear)
            {
                for (std::uint32_t tap = 0; tap < room::HrtfSet::K_TAP_COUNT; ++tap)
                {
                    put_f32(tap == 0 ? 1.0f : 0.0f);
                }
            }
        }
        const std::size_t zero_floats =
            2u * K_CHANNELS * room::HrtfSet::K_DECODER_TAPS + 2u * room::HrtfSet::K_DIFFUSE_TAPS;
        bytes.insert(bytes.end(), zero_floats * sizeof(float), 0);
        return bytes;
    }
} // namespace SushiDSP::test
