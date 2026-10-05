/****************************************************************/
/* SplitterNode.hpp                                             */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

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
        void set_parameter_for_control_thread(const char* id, float value) override;

        /// @brief This splitter's registered type id.
        /// @return "sushidsp.splitter".
        const char* type_id() const override;
    };

    /// @brief The splitter's product registration entry point.
    /// @return The static NodeDescriptor for "sushidsp.splitter".
    const NodeDescriptor& splitter_descriptor();
}
