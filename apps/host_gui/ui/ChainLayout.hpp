/**************************************************************************/
/* ChainLayout.hpp                                                        */
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

#include <cstddef>
#include <utility>
#include <vector>

#include "RigController.hpp"

namespace ui
{
    /// @brief One slot's place on the strip's grid.
    struct ChainCell
    {
        RigController::SlotId slot;
        int                   row;
        int                   column;
    };

    /// @brief One edge, as a pair of indices into ChainView::cells.
    struct ChainLink
    {
        std::size_t from_cell;
        std::size_t to_cell;
    };

    /// @brief Where every slot sits and which cells each edge joins.
    struct ChainView
    {
        std::vector<ChainCell> cells;
        std::vector<ChainLink> links;
        int                    row_count;
        int                    column_count;
    };

    /// @brief Places every slot on a row and column grid for drawing. A node's column is one past
    /// its deepest upstream, so a rejoin lands right of the longest branch. Row is 0 along a
    /// linear run; a node's second downstream edge starts row 1 and everything after it on that
    /// branch stays on row 1, and a node with two or more upstream edges rejoins at row 0.
    /// @param slots Every live slot.
    /// @param edges Every edge, as (from, to) pairs.
    /// @return The grid; cells is empty and both counts are 0 for an empty rig. Cells come out in
    /// topological order.
    ChainView layout_chain(const std::vector<RigController::SlotId>& slots,
                           const std::vector<std::pair<RigController::SlotId, RigController::SlotId>>& edges);
}
