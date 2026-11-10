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

#include <memory>

#include <SushiDSP/nodes/room/hrtf_set.hpp>

namespace SushiDSP::room
{
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
