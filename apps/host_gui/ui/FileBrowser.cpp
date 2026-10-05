/****************************************************************/
/* FileBrowser.cpp                                              */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file FileBrowser.cpp
 * @brief Implements the directory browser that reports the file the user picked.
 * @author Mustafa Garip
 */

#include "ui/FileBrowser.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <vector>

#include <imgui.h>

#include "ui/DirectoryListing.hpp"
#include "ui/Theme.hpp"

namespace ui
{
    namespace
    {
        /// @brief The listing's height in pixels, so a folder of two hundred impulse responses
        /// scrolls instead of stretching the panel.
        constexpr float kListingHeight = 220.0f;

        /// @brief Names the directory one level above another.
        /// @param directory The directory; a trailing separator is ignored.
        /// @return The parent, or std::nullopt when directory is a filesystem root.
        std::optional<std::string> parent_directory(const std::string& directory)
        {
            std::filesystem::path here(directory);
            if (!here.has_filename())
            {
                here = here.parent_path();
            }
            const std::filesystem::path up = here.parent_path();
            if (up.empty() || up == here)
            {
                return std::nullopt;
            }
            return up.string();
        }

        /// @brief Joins a directory and one of its entries into a full path.
        /// @param directory The directory holding the entry.
        /// @param name The entry's own name.
        /// @return The full path.
        std::string child_path(const std::string& directory, const std::string& name)
        {
            return (std::filesystem::path(directory) / name).string();
        }

        /// @brief The payload type a dragged take carries, accepted by ui::DrawArrangementView.
        constexpr const char* kTakePayload = "TAKE_FILE_PATH";

        /// @brief Makes the row the caller has just drawn a drag source carrying a take's path.
        /// The path travels with its terminator, so the target reads it as a C string.
        /// @param path The file's full path.
        /// @param name The file's own name, shown under the pointer while it is dragged.
        void offer_as_take(const std::string& path, const std::string& name)
        {
            if (!is_take_file(path) || !ImGui::BeginDragDropSource())
            {
                return;
            }

            ImGui::SetDragDropPayload(kTakePayload, path.c_str(), path.size() + 1u);
            ImGui::TextColored(to_imvec4(palette().text), "%s", name.c_str());
            ImGui::EndDragDropSource();
        }

        /// @brief Draws one clickable row in the listing's own color.
        /// @param label The text to show.
        /// @param color The row's color.
        /// @return True on the frame the row is clicked.
        bool listing_row(const std::string& label, Color color)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, to_imvec4(color));
            const bool clicked = ImGui::Selectable(label.c_str());
            ImGui::PopStyleColor();
            return clicked;
        }
    }

    bool is_take_file(const std::string& path)
    {
        std::string carried = std::filesystem::path(path).extension().string();
        std::transform(carried.begin(), carried.end(), carried.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return carried == ".wav" || carried == ".mp3";
    }

    std::optional<std::string> FileBrowser(const char* id, std::string& directory,
                                           const std::string& extension)
    {
        return FileBrowser(id, directory, std::vector<std::string>{ extension });
    }

    std::optional<std::string> FileBrowser(const char* id, std::string& directory,
                                           const std::vector<std::string>& extensions)
    {
        std::optional<std::string> picked;
        std::optional<std::string> next_directory;
        const std::string current = directory;

        ImGui::PushID(id);
        ImGui::TextColored(to_imvec4(palette().muted_text), "%s",
                           current.empty() ? "(no folder)" : current.c_str());
        ImGui::BeginChild("listing", ImVec2(0.0f, kListingHeight), ImGuiChildFlags_Border);

        int row = 0;
        ImGui::PushID(row++);
        if (listing_row("..", palette().accent))
        {
            next_directory = parent_directory(current);
        }
        ImGui::PopID();

        for (const DirectoryEntry& entry : list_directory(current, extensions))
        {
            ImGui::PushID(row++);
            if (entry.is_directory)
            {
                if (listing_row(entry.name + "/", palette().accent))
                {
                    next_directory = child_path(current, entry.name);
                }
            }
            else
            {
                const std::string full = child_path(current, entry.name);
                if (listing_row(entry.name, palette().text))
                {
                    picked = full;
                }
                offer_as_take(full, entry.name);
            }
            ImGui::PopID();
        }

        ImGui::EndChild();
        ImGui::PopID();

        if (next_directory.has_value())
        {
            directory = *next_directory;
        }
        return picked;
    }
}
