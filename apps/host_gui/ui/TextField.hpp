/****************************************************************/
/* TextField.hpp                                                */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

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
