/**************************************************************************/
/* SplitterPicker.cpp                                                    */
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

#include "ui/SplitterPicker.hpp"

#include <exception>
#include <optional>
#include <stdexcept>
#include <vector>

#include <imgui.h>

#include "ui/Theme.hpp"

namespace ui
{
    void DrawSplitterPicker(RigController& rig, RigController::SlotId chain_end,
                             std::string& error)
    {
        ImGui::BeginGroup();
        ImGui::TextUnformatted("Split");
        if (ImGui::Button("Split##splitter_picker"))
        {
            // A rejected republish leaves the graph the audio thread already plays untouched
            // (RigController::publish() compiles before it swaps), so a thrown edit here is
            // caught and reported rather than let past ImGui's unmatched End().
            //
            // add_branch()'s mixer-creation path only triggers once the splitter already has
            // a downstream branch (RigController::add_branch, the `down.empty()` check), so
            // the first call here only gives the splitter its lone branch; the second call is
            // what turns that into an actual parallel split with a rejoining mixer.
            std::vector<RigController::SlotId> created;
            try
            {
                const RigController::SlotId splitter = rig.add_node("sushidsp.splitter", false);
                created.push_back(splitter);
                rig.connect(chain_end, splitter, false);

                const std::optional<RigController::SlotId> first_branch =
                    rig.add_branch(splitter, "sushidsp.splitter");
                if (!first_branch.has_value())
                {
                    throw std::runtime_error("Could not create the parallel branch");
                }
                created.push_back(*first_branch);

                const std::optional<RigController::SlotId> second_branch =
                    rig.add_branch(splitter, "sushidsp.splitter");
                if (!second_branch.has_value())
                {
                    throw std::runtime_error("Could not create the parallel branch");
                }
                created.push_back(*second_branch);

                error.clear();
            }
            catch (const std::exception& failure)
            {
                error = failure.what();

                // Undo whatever this attempt created, most recently added first, so remove()'s
                // upstream/downstream reconnect never has to bridge across an already-removed
                // slot; this restores the graph exactly as it stood before the click.
                for (auto slot = created.rbegin(); slot != created.rend(); ++slot)
                {
                    rig.remove(*slot, false);
                }
            }
        }
        ImGui::EndGroup();
    }
}
