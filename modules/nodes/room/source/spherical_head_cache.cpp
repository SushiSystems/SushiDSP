/**************************************************************************/
/* spherical_head_cache.cpp                                               */
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
 * @file spherical_head_cache.cpp
 * @brief Implements the per-rate cache of the rigid-sphere HrtfSet.
 * @author Mustafa Garip & Sushi Systems
 */

#include <SushiDSP/nodes/room/spherical_head_cache.hpp>

#include <utility>

#include <SushiDSP/nodes/room/spherical_head_hrtf.hpp>

namespace SushiDSP::room
{
    HrtfRateCache::HrtfRateCache(Builder builder) : builder_(std::move(builder)) {}

    std::shared_ptr<const HrtfSet> HrtfRateCache::shared(double sample_rate)
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        std::shared_ptr<const HrtfSet>& slot = table_[sample_rate];
        if (!slot)
        {
            try
            {
                slot = std::make_shared<const HrtfSet>(builder_(sample_rate));
            }
            catch (...)
            {
                table_.erase(sample_rate);
                throw;
            }
        }
        return slot;
    }

    std::shared_ptr<const HrtfSet> SphericalHeadCache::shared(double sample_rate)
    {
        static HrtfRateCache cache(&SphericalHeadHrtf::build);
        return cache.shared(sample_rate);
    }
} // namespace SushiDSP::room
