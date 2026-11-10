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

#include <map>
#include <mutex>

#include <SushiDSP/nodes/room/spherical_head_hrtf.hpp>

namespace SushiDSP::room
{
    namespace
    {
        /// @brief Guards the table of built sets.
        std::mutex& table_mutex()
        {
            static std::mutex instance;
            return instance;
        }

        /// @brief Holds the built sets by sample rate.
        std::map<double, std::shared_ptr<const HrtfSet>>& table()
        {
            static std::map<double, std::shared_ptr<const HrtfSet>> instance;
            return instance;
        }
    } // namespace

    std::shared_ptr<const HrtfSet> SphericalHeadCache::shared(double sample_rate)
    {
        const std::lock_guard<std::mutex> lock(table_mutex());
        std::shared_ptr<const HrtfSet>& slot = table()[sample_rate];
        if (!slot)
        {
            try
            {
                slot = std::make_shared<const HrtfSet>(SphericalHeadHrtf::build(sample_rate));
            }
            catch (...)
            {
                table().erase(sample_rate);
                throw;
            }
        }
        return slot;
    }
} // namespace SushiDSP::room
