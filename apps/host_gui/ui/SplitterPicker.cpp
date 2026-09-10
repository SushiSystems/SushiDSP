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
            try
            {
                const RigController::SlotId splitter = rig.add_node("sushidsp.splitter", false);
                rig.connect(chain_end, splitter, false);
                rig.add_branch(splitter, "sushidsp.splitter");
                error.clear();
            }
            catch (const std::exception& failure)
            {
                error = failure.what();
            }
        }
        ImGui::EndGroup();
    }
}
