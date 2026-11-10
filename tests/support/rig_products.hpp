/**************************************************************************/
/* rig_products.hpp                                                       */
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
 * @file rig_products.hpp
 * @brief Declares the fixture that builds rigs on a catalogue the test owns, not on the node registry.
 * @author Mustafa Garip
 */

#pragma once

#include <cstddef>
#include <cstring>
#include <limits>
#include <utility>

#include <gtest/gtest.h>

#include <SushiDSP/nodes/cabinet/cabinet_node.hpp>
#include <SushiDSP/nodes/dd3/dd3_delay.hpp>
#include <SushiDSP/nodes/jcm800/jcm800.hpp>
#include <SushiDSP/nodes/microphone/microphone_node.hpp>
#include <SushiDSP/nodes/peavey5150/peavey5150.hpp>
#include <SushiDSP/nodes/room/room_node.hpp>
#include <SushiDSP/nodes/routing/bus_node.hpp>
#include <SushiDSP/nodes/routing/channel_strip_node.hpp>
#include <SushiDSP/nodes/routing/mixer_node.hpp>
#include <SushiDSP/nodes/routing/splitter_node.hpp>
#include <SushiDSP/nodes/routing/stereo_strip_node.hpp>
#include <SushiDSP/nodes/tubescreamer/tube_screamer.hpp>
#include <SushiDSP/session/rig_preset/node_catalogue.hpp>

#include "rig_controller.hpp"

namespace TestSupport
{
    /// @brief Fixture base that hands each test a catalogue of the products the rig tests build and
    /// the rigs made on it, so no rig test reaches the process-wide node registry.
    class RigProducts : public ::testing::Test
    {
        public:
            RigProducts()
            {
                catalogue_.add(SushiDSP::tube_screamer_descriptor());
                catalogue_.add(SushiDSP::jcm800_descriptor());
                catalogue_.add(SushiDSP::peavey5150_descriptor());
                catalogue_.add(SushiDSP::dd3_descriptor());
                catalogue_.add(SushiDSP::splitter_descriptor());
                catalogue_.add(SushiDSP::mixer_descriptor());
                catalogue_.add(SushiDSP::channel_strip_descriptor());
                catalogue_.add(SushiDSP::stereo_strip_descriptor());
                catalogue_.add(SushiDSP::bus_descriptor());
                catalogue_.add(SushiDSP::cabinet_descriptor());
                catalogue_.add(SushiDSP::microphone_descriptor());
                catalogue_.add(SushiDSP::room::room_descriptor());
            }

        protected:
            /// @brief Returns the catalogue of the products above.
            const SushiDSP::NodeCatalogue& catalogue() const noexcept
            {
                return catalogue_;
            }

            /// @brief Builds a rig on the catalogue, with its own timeline.
            /// @param info The sample rate, block size and channel count.
            /// @param clock Returns the present time for the room publish interval; empty reads the
            /// steady clock.
            /// @return The rig; it must not outlive the fixture.
            RigController rig(const SushiDSP::PrepareInfo& info,
                              RigController::ClockSource clock = {}) const
            {
                return RigController(info, std::move(clock), catalogue_);
            }

            /// @brief Reads the default a product declares for one of its parameters.
            /// @param descriptor_id The product's id.
            /// @param parameter_id The parameter's id.
            /// @return The declared default, or NaN when the catalogue or the product lacks it.
            float default_of(const char* descriptor_id, const char* parameter_id) const
            {
                const SushiDSP::NodeDescriptor* descriptor = catalogue_.find(descriptor_id);
                if (descriptor == nullptr)
                {
                    return std::numeric_limits<float>::quiet_NaN();
                }
                for (std::size_t p = 0; p < descriptor->parameter_count; ++p)
                {
                    if (std::strcmp(descriptor->parameters[p].id, parameter_id) == 0)
                    {
                        return descriptor->parameters[p].default_value;
                    }
                }
                return std::numeric_limits<float>::quiet_NaN();
            }

        private:
            SushiDSP::InMemoryNodeCatalogue catalogue_;
    };
} // namespace TestSupport
