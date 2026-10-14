/**************************************************************************/
/* splitter_node.hpp                                                      */
/**************************************************************************/
/*                          This file is part of:                         */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                         https://sushisystems.io                        */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the PolyForm Noncommercial License 1.0.0 (the           */
/* "License"); you may not use this file except in compliance with the    */
/* License. You may obtain a copy of the License at                       */
/*                                                                        */
/*     https://polyformproject.org/licenses/noncommercial/1.0.0           */
/*                                                                        */
/* Noncommercial use is free. Commercial use requires a separate licence  */
/* from Sushi Systems; see COMMERCIAL.md. The software is provided        */
/* "as is", without warranty of any kind.                                 */
/**************************************************************************/

/**
 * @file splitter_node.hpp
 * @brief Declares the pass-through node that names a split point in a rig.
 * @author Mustafa Garip
 */

#pragma once

#include <SushiDSP/foundation/core/audio_node.hpp>
#include <SushiDSP/foundation/core/node_descriptor.hpp>

namespace SushiDSP
{
    /// @brief A routing pass-through: copies its single input to its single output. Fan-out to
    /// multiple downstream nodes is entirely RigGraph's job (a RigGraph node's output buffer can
    /// already be read by more than one downstream connect() edge) — this node exists only so a
    /// rig can name an explicit "split point" in the gear panel / preset graph, not because it
    /// duplicates the buffer itself.
    class SplitterNode : public AudioNode
    {
        public:
            void prepare(const PrepareInfo& info) override;
            void process(ProcessContext& context) override;
            PortLayout ports() const override;
            void reset() override;
            float get_parameter(const char* id) const override;
            void set_parameter_for_control_thread(const char* id, float value) override;

            /// @brief This splitter's registered type id.
            /// @return "sushidsp.splitter".
            const char* type_id() const override;
    };

    /// @brief The splitter's product registration entry point.
    /// @return The static NodeDescriptor for "sushidsp.splitter".
    const NodeDescriptor& splitter_descriptor();
} // namespace SushiDSP
