/**************************************************************************/
/* TextField.hpp                                                          */
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
 * @file TextField.hpp
 * @brief Defines a text box whose buffer follows its owner's value while idle.
 * @author Mustafa Garip
 */

#pragma once

#include <cstddef>
#include <cstring>
#include <string>

#include <imgui.h>

namespace ui
{
    /// @brief Draws a text box whose buffer follows an owner's value whenever the pointer is not
    /// in it, so a value the owner replaced (by loading a preset, say) reaches the screen.
    /// @param id The ImGui id, unique within the window.
    /// @param buffer The frame-to-frame edit buffer.
    /// @param capacity The buffer's size in bytes, terminator included.
    /// @param owned The owner's current value, copied in while the box is idle.
    /// @return True on the frames where the user changed the buffer.
    inline bool TextField(const char* id, char* buffer, std::size_t capacity,
                          const std::string& owned)
    {
        const bool changed = ImGui::InputText(id, buffer, capacity);
        if (!changed && !ImGui::IsItemActive() && owned != buffer && owned.size() < capacity)
        {
            std::memcpy(buffer, owned.c_str(), owned.size() + 1);
        }
        return changed;
    }
}
