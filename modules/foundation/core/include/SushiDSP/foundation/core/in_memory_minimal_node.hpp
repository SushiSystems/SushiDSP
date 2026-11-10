/**************************************************************************/
/* in_memory_minimal_node.hpp                                             */
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
 * @file in_memory_minimal_node.hpp
 * @brief Declares the in-memory AudioNode that overrides only what AudioNode requires.
 * @author Mustafa Garip
 * @see modules/foundation/core/README.md
 */

#pragma once

#include <cstdint>

#include <SushiDSP/foundation/core/audio_node.hpp>

namespace SushiDSP
{
    /// @brief An AudioNode that overrides the pure virtual methods and nothing else.
    ///
    /// It keeps every promise of AudioNode: process() writes silence to every channel of port 0, and
    /// get_parameter() answers 0 for every id. It leaves every virtual that has a default to
    /// AudioNode, so a test reaches those defaults through it; InMemoryNode overrides
    /// set_parameter_for_audio_thread() and cannot.
    class InMemoryMinimalNode final : public AudioNode
    {
        public:
            /// @brief Makes a node of @p layout.
            /// @param layout The input and output channel counts the node declares.
            explicit InMemoryMinimalNode(PortLayout layout) : layout_(layout)
            {
            }

            /// @brief Does nothing.
            void prepare(const PrepareInfo&) override
            {
            }

            /// @brief Writes silence to every output channel of port 0.
            void process(ProcessContext& context) override
            {
                for (std::uint32_t channel = 0; channel < layout_.output_channels; ++channel)
                {
                    for (std::uint32_t frame = 0; frame < context.frames; ++frame)
                    {
                        context.outputs[channel][frame] = 0.0f;
                    }
                }
            }

            /// @brief Returns the declared layout.
            PortLayout ports() const override
            {
                return layout_;
            }

            /// @brief Answers 0 for every id.
            float get_parameter(const char*) const override
            {
                return 0.0f;
            }

            /// @brief Changes nothing.
            void set_parameter_for_control_thread(const char*, float) override
            {
            }

            /// @brief Does nothing.
            void reset() override
            {
            }

        private:
            /// @brief The declared layout.
            PortLayout layout_;
    };
} // namespace SushiDSP
