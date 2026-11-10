/**************************************************************************/
/* spherical_head_cache.hpp                                               */
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
 * @file spherical_head_cache.hpp
 * @brief A process-wide cache of the rigid-sphere HrtfSet, one per sample rate.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <functional>
#include <map>
#include <memory>
#include <mutex>

#include <SushiDSP/nodes/room/hrtf_set.hpp>

namespace SushiDSP::room
{
    /// @brief Builds an HrtfSet once per sample rate through the builder it is given and hands the
    /// same immutable set to every caller. Thread-safe. It is the keyed table behind
    /// SphericalHeadCache, kept apart so a test can hand it a cheap builder and read the identity
    /// the cache promises without paying for the spherical head.
    class HrtfRateCache
    {
        public:
            /// @brief Names what builds the set for one rate.
            using Builder = std::function<HrtfSet(double)>;

            /// @brief Stores the builder.
            /// @param builder Called once per rate, on the first request for it.
            explicit HrtfRateCache(Builder builder);

            /// @brief Returns the set the builder produced at the rate, built on the first request
            /// and shared by every later one.
            /// @throw whatever the builder throws; nothing is cached then.
            std::shared_ptr<const HrtfSet> shared(double sample_rate);

        private:
            /// @brief Stores the builder.
            Builder builder_;

            /// @brief Guards the table.
            std::mutex mutex_;

            /// @brief Holds the built sets by sample rate.
            std::map<double, std::shared_ptr<const HrtfSet>> table_;
    };

    /// @brief Builds the rigid-sphere HrtfSet once per sample rate and hands the same immutable
    /// set to every caller, because the build costs seconds in a debug build. Thread-safe. Call it
    /// from a control thread only: it locks and may allocate, so never from process().
    class SphericalHeadCache
    {
        public:
            /// @brief Returns the set SphericalHeadHrtf::build produces at the rate, built on the
            /// first request and shared by every later one. The set equals a fresh build bit for bit.
            /// @throw whatever SphericalHeadHrtf::build throws; nothing is cached then.
            static std::shared_ptr<const HrtfSet> shared(double sample_rate);
    };
} // namespace SushiDSP::room
