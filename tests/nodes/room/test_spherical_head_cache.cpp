/**************************************************************************/
/* test_spherical_head_cache.cpp                                          */
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
 * @file test_spherical_head_cache.cpp
 * @brief Tests for SphericalHeadCache, the per-rate shared rigid-sphere set.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

#include <SushiDSP/nodes/room/spherical_head_cache.hpp>
#include <SushiDSP/nodes/room/spherical_head_hrtf.hpp>

namespace
{
    /// @brief Holds the rate the sets are requested at.
    constexpr double K_RATE = 48000.0;

    /// @brief Holds a second rate, to show the cache is keyed by it.
    constexpr double K_OTHER_RATE = 44100.0;

    /// @brief Returns whether two float ranges hold the same bytes.
    bool same_bytes(const float* a, const float* b, std::size_t count)
    {
        return std::memcmp(a, b, count * sizeof(float)) == 0;
    }
} // namespace

TEST(Small_SphericalHeadCache, TwoRequestsAtOneRateReturnTheSameSetAndAnotherRateAnotherSet)
{
    const auto first = SushiDSP::room::SphericalHeadCache::shared(K_RATE);
    const auto second = SushiDSP::room::SphericalHeadCache::shared(K_RATE);
    const auto other = SushiDSP::room::SphericalHeadCache::shared(K_OTHER_RATE);

    EXPECT_EQ(first.get(), second.get());
    EXPECT_NE(first.get(), other.get());
    EXPECT_DOUBLE_EQ(other->sample_rate(), K_OTHER_RATE);
}

TEST(Medium_SphericalHeadCache, TheCachedSetEqualsAFreshBuildBitForBit)
{
    const auto cached = SushiDSP::room::SphericalHeadCache::shared(K_RATE);
    const SushiDSP::room::HrtfSet fresh = SushiDSP::room::SphericalHeadHrtf::build(K_RATE);

    ASSERT_EQ(cached->direction_count(), fresh.direction_count());
    EXPECT_DOUBLE_EQ(cached->sample_rate(), fresh.sample_rate());
    EXPECT_EQ(cached->decoder_order(), fresh.decoder_order());
    ASSERT_EQ(cached->decoder_filters().size(), fresh.decoder_filters().size());
    EXPECT_TRUE(same_bytes(cached->decoder_filters().data(), fresh.decoder_filters().data(),
                           fresh.decoder_filters().size()));
    for (std::uint32_t ear = 0; ear < 2u; ++ear)
    {
        EXPECT_TRUE(same_bytes(cached->diffuse_field(ear).data(), fresh.diffuse_field(ear).data(),
                               SushiDSP::room::HrtfSet::K_DIFFUSE_TAPS));
    }
    for (std::uint32_t index = 0; index < fresh.direction_count(); ++index)
    {
        const auto& a = cached->direction(index);
        const auto& b = fresh.direction(index);
        EXPECT_EQ(std::memcmp(&a, &b, sizeof(a)), 0) << "direction " << index;
        const auto pair_a = cached->interpolate(a.azimuth_rad, a.elevation_rad);
        const auto pair_b = fresh.interpolate(b.azimuth_rad, b.elevation_rad);
        EXPECT_TRUE(same_bytes(pair_a.left.data(), pair_b.left.data(),
                               SushiDSP::room::HrtfSet::K_TAP_COUNT))
            << "left " << index;
        EXPECT_TRUE(same_bytes(pair_a.right.data(), pair_b.right.data(),
                               SushiDSP::room::HrtfSet::K_TAP_COUNT))
            << "right " << index;
    }
}
