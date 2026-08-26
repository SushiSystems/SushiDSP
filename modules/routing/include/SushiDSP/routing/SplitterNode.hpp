/**************************************************************************/
/* SplitterNode.hpp                                                       */
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

#pragma once

#include "SushiDSP/core/IAudioNode.hpp"
#include "SushiDSP/core/NodeDescriptor.hpp"

namespace SushiDSP
{
    /// @brief A routing pass-through: copies its single input to its single output. Fan-out to
    /// multiple downstream nodes is entirely RigGraph's job (a RigGraph node's output buffer can
    /// already be read by more than one downstream connect() edge) — this node exists only so a
    /// rig can name an explicit "split point" in the gear panel / preset graph, not because it
    /// duplicates the buffer itself.
    class SplitterNode : public IAudioNode
    {
    public:
        void prepare(const PrepareInfo& info) override;
        void process(ProcessContext& context) override;
        PortLayout ports() const override;
        void reset() override;
        float get_parameter(const char* id) const override;
        void set_parameter(const char* id, float value) override;

        /// @brief This splitter's registered type id.
        /// @return "sushidsp.splitter".
        const char* type_id() const override;
    };

    /// @brief The splitter's product registration entry point.
    /// @return The static NodeDescriptor for "sushidsp.splitter".
    const NodeDescriptor& splitter_descriptor();
}
