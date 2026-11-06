/**************************************************************************/
/* parameter_routing.cpp                                                  */
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
 * @file parameter_routing.cpp
 * @brief Implements the table the audio thread reads to reach a rig parameter's node.
 * @author Mustafa Garip
 */

#include "parameter_routing.hpp"

#include <numeric>

#include "rig_parameter.hpp"

namespace SushiDSP::Plugin
{
    namespace
    {
        /// @brief Adds up the parameter counts of every slot.
        std::size_t total_of(const std::vector<std::uint32_t>& parameters_per_slot)
        {
            return std::accumulate(parameters_per_slot.begin(), parameters_per_slot.end(),
                                   std::size_t{0});
        }
    } // namespace

    ParameterRouting::ParameterRouting(const RigGraph* graph,
                                       const std::vector<std::uint32_t>& parameters_per_slot)
        : graph_(graph), routes_(total_of(parameters_per_slot))
    {
        slot_offsets_.reserve(parameters_per_slot.size() + 1);
        std::size_t offset = 0;
        for (std::uint32_t count : parameters_per_slot)
        {
            slot_offsets_.push_back(offset);
            offset += count;
        }
        slot_offsets_.push_back(offset);
    }

    const RigGraph* ParameterRouting::graph() const noexcept
    {
        return graph_;
    }

    std::size_t ParameterRouting::size() const noexcept
    {
        return routes_.size();
    }

    ParameterRouting::Route* ParameterRouting::find(std::uint32_t id) noexcept
    {
        const std::size_t slot = rig_parameter_slot(id);
        if (slot + 1 >= slot_offsets_.size())
        {
            return nullptr;
        }
        const std::size_t position = slot_offsets_[slot] + rig_parameter_index(id);
        return position < slot_offsets_[slot + 1] ? &routes_[position] : nullptr;
    }

    ParameterRouting::Route& ParameterRouting::at(std::size_t position) noexcept
    {
        return routes_[position];
    }
} // namespace SushiDSP::Plugin
