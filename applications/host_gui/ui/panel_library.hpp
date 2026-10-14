/**************************************************************************/
/* panel_library.hpp                                                      */
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
 * @file panel_library.hpp
 * @brief Declares the front-panel descriptors and the table that looks them up.
 * @author Mustafa Garip
 */

#pragma once

#include <cstddef>
#include <vector>

#include <SushiDSP/foundation/core/node_descriptor.hpp>

#include "ui/geometry.hpp"

namespace ui
{
    /// @brief Which widget a placement draws.
    enum class ControlKind
    {
        Knob,
        Switch,
        Readout,

        /// @brief A vertical fader, whose size is its travel as a fraction of panel height.
        Slider
    };

    /// @brief One control's place on a panel. position is the control's center in [0, 1] on both
    /// axes and size is its diameter as a fraction of panel height, so a panel scales with the
    /// window without being laid out again. A Slider reads size as its travel rather than its
    /// diameter and takes its width from that travel.
    struct ControlPlacement
    {
            const char* param_id;
            ControlKind kind;
            Vec2 position;
            float size;
            const char* caption; ///< nullptr means use ParameterDescriptor::label.
    };

    /// @brief A panel's colors. texture_id is the hook a later bitmap-skin change fills in; every
    /// panel defined today leaves it nullptr and the drawing code ignores it.
    struct PanelSkin
    {
            Color body;
            Color face;
            Color accent;
            Color text;
            const char* texture_id;
    };

    /// @brief Everything the stage needs to draw one node type's front panel.
    struct PanelDescriptor
    {
            const char* descriptor_id;
            Vec2 aspect; ///< Width and height in arbitrary units; only the ratio matters.
            PanelSkin skin;
            const ControlPlacement* controls;
            std::size_t control_count;

            /// @brief Optional per-control visibility rule; nullptr draws every control.
            /// @param control_index The control's index in controls.
            /// @param upstream_count How many slots feed this node.
            /// @return True to draw the control.
            bool (*is_control_visible)(std::size_t control_index, std::size_t upstream_count);
    };

    /// @brief Every hand-authored panel.
    /// @return The table, for callers that need to walk it rather than look one entry up.
    const std::vector<PanelDescriptor>& panel_table();

    /// @brief Looks up the hand-authored panel for a node type.
    /// @param descriptor_id The NodeDescriptor id.
    /// @return The panel, or nullptr when the table has no entry for it.
    const PanelDescriptor* panel_for(const char* descriptor_id);

    /// @brief Builds a generic grid panel for a node type with no hand-authored entry, laying its
    /// parameters out row-major across ceil(sqrt(parameter_count)) columns.
    /// @param node The node type to lay out.
    /// @param storage Receives the generated placements and must outlive the returned descriptor,
    /// which points into it.
    /// @return The generated panel.
    PanelDescriptor auto_panel(const SushiDSP::NodeDescriptor& node,
                               std::vector<ControlPlacement>& storage);
} // namespace ui
