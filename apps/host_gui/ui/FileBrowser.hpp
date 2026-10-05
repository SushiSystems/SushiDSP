/****************************************************************/
/* FileBrowser.hpp                                              */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file FileBrowser.hpp
 * @brief Declares the directory browser that reports the file the user picked.
 * @author Mustafa Garip
 */

#pragma once

#include <optional>
#include <string>
#include <vector>

namespace ui
{
    /// @brief Draws a directory listing the user can walk, and reports the file they picked.
    /// @param id The ImGui id, unique within the window.
    /// @param directory In/out: the directory on show. The browser rewrites it when the user
    /// opens a subdirectory or goes up, so the caller can remember where they were.
    /// @param extension The extension to offer, e.g. ".wav".
    /// @return The picked file's full path, or std::nullopt on every frame the user picks
    /// nothing.
    std::optional<std::string> FileBrowser(const char* id, std::string& directory,
                                           const std::string& extension);

    /// @brief Draws a directory listing offering several extensions at once, so the takes
    /// browser can show songs beside recordings.
    /// @param id The ImGui id, unique within the window.
    /// @param directory In/out: the directory on show, rewritten as the user walks it.
    /// @param extensions The extensions to offer, e.g. { ".wav", ".mp3" }.
    /// @return The picked file's full path, or std::nullopt on every frame the user picks
    /// nothing.
    std::optional<std::string> FileBrowser(const char* id, std::string& directory,
                                           const std::vector<std::string>& extensions);

    /// @brief Reports whether a file may be dragged onto an arrangement lane.
    /// @param path The file's path; its extension is read case-insensitively.
    /// @return True for a WAV or an mp3.
    bool is_take_file(const std::string& path);
}
