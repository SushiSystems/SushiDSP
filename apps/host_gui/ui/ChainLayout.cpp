/**************************************************************************/
/* ChainLayout.cpp                                                        */
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

#include "ui/ChainLayout.hpp"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace ui
{
    ChainView layout_chain(const std::vector<SlotId>& slots,
                           const std::vector<std::pair<SlotId, SlotId>>& edges)
    {
        ChainView view{ {}, {}, 0, 0 };
        if (slots.empty())
        {
            return view;
        }

        std::unordered_map<SlotId, std::vector<SlotId>> downstream;
        std::unordered_map<SlotId, std::size_t> in_degree;
        std::unordered_map<SlotId, std::size_t> upstream_count;
        for (SlotId slot : slots)
        {
            in_degree[slot] = 0;
            upstream_count[slot] = 0;
        }
        for (const auto& edge : edges)
        {
            if (in_degree.count(edge.first) == 0 || in_degree.count(edge.second) == 0)
            {
                continue;
            }
            downstream[edge.first].push_back(edge.second);
            ++in_degree[edge.second];
            ++upstream_count[edge.second];
        }

        std::unordered_map<SlotId, int> column;
        std::unordered_map<SlotId, int> row;

        // Kahn's algorithm, seeded in the caller's slot order so the result is deterministic.
        std::vector<SlotId> ready;
        int next_free_row = 0;
        for (SlotId slot : slots)
        {
            if (in_degree[slot] == 0)
            {
                // Each root starts its own row, so two rigs sharing the view do not overlap.
                row[slot] = next_free_row++;
                ready.push_back(slot);
            }
        }

        std::vector<SlotId> ordered;
        ordered.reserve(slots.size());
        for (std::size_t head = 0; head < ready.size(); ++head)
        {
            const SlotId current = ready[head];
            ordered.push_back(current);

            const auto children = downstream.find(current);
            if (children == downstream.end())
            {
                continue;
            }
            for (std::size_t branch = 0; branch < children->second.size(); ++branch)
            {
                const SlotId child = children->second[branch];
                column[child] = std::max(column[child], column[current] + 1);
                // A rejoin returns to row 0; the first branch inherits, later branches drop to 1.
                if (upstream_count[child] < 2)
                {
                    row[child] = branch == 0 ? row[current] : 1;
                }
                if (--in_degree[child] == 0)
                {
                    ready.push_back(child);
                }
            }
        }

        // A slot in a cycle never reaches in_degree 0, so the walk leaves it out; park it.
        int deepest_column = 0;
        for (SlotId slot : ordered)
        {
            deepest_column = std::max(deepest_column, column[slot]);
        }
        const std::unordered_set<SlotId> scheduled(ordered.begin(), ordered.end());
        for (SlotId slot : slots)
        {
            if (scheduled.count(slot) == 0)
            {
                column[slot] = deepest_column;
                row[slot] = next_free_row++;
                ordered.push_back(slot);
            }
        }

        view.cells.reserve(ordered.size());
        std::unordered_map<SlotId, std::size_t> cell_index;
        for (SlotId slot : ordered)
        {
            cell_index[slot] = view.cells.size();
            view.cells.push_back({ slot, row[slot], column[slot] });
            view.row_count = std::max(view.row_count, row[slot] + 1);
            view.column_count = std::max(view.column_count, column[slot] + 1);
        }

        for (const auto& edge : edges)
        {
            const auto from = cell_index.find(edge.first);
            const auto to = cell_index.find(edge.second);
            if (from != cell_index.end() && to != cell_index.end())
            {
                view.links.push_back({ from->second, to->second });
            }
        }

        return view;
    }
}
