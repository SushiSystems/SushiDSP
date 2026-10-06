/**************************************************************************/
/* RigChainView.hpp                                                       */
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

/** @file RigChainView.hpp
 *  @brief Declares the chain view: the chain-type rail and the routed rig chain beside it.
 *  @author Mustafa Garip
 */

#pragma once

#include <optional>
#include <string>

#include "RigController.hpp"
#include "RigTopology.hpp"
#include "TopologyApplier.hpp"
#include "ui/Geometry.hpp"

namespace ui
{
    /// @brief Holds what the chain view keeps between frames: the topology, its applier, the
    /// selection and the last refused edit's message.
    struct RigChainState
    {
        RigTopology           topology;   ///< Holds the chain type and every stage's slots.
        TopologyApplier       applier;    ///< Holds the routing slots and parked snapshots.
        std::optional<SlotId> selected;   ///< Holds the selected slot, cleared when it vanishes.
        std::string           edit_error; ///< Holds why the last edit was refused, or "".
    };

    /// @brief Draws the chain-type rail and the chain, and applies every edit made on them.
    /// @param rig The rig to draw and edit.
    /// @param state In/out: the view's topology, applier, selection and edit error.
    /// @param created Out: the slot a gear drop created this frame, or std::nullopt.
    void DrawRigChainView(RigController& rig, RigChainState& state, std::optional<SlotId>& created);
}
