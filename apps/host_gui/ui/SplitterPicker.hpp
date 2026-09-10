/**************************************************************************/
/* SplitterPicker.hpp                                                    */
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

#include "RigController.hpp"

namespace ui
{
    /// @brief Draws the splitter picker at the rig design view's left edge. Clicking it inserts
    /// a splitter after `chain_end` and automatically adds the matching mixer, with no manual
    /// node placement or connect step from the user.
    /// @param rig The rig to edit.
    /// @param chain_end The slot to split after — typically the last slot in chain order.
    void DrawSplitterPicker(RigController& rig, RigController::SlotId chain_end);
}
