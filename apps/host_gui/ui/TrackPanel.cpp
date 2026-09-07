/**************************************************************************/
/* TrackPanel.cpp                                                         */
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

#include "ui/TrackPanel.hpp"

#include <array>
#include <cstddef>
#include <optional>

#include <imgui.h>

#include "ui/FileBrowser.hpp"
#include "ui/TextField.hpp"
#include "ui/Theme.hpp"

namespace ui
{
    namespace
    {
        /// @brief How many tracks the panel lays out, one row each.
        constexpr std::size_t kTrackCount = SushiDSP::TrackPlayer::kMaxTracks;

        /// @brief Characters a track's name box holds, terminator included.
        constexpr std::size_t kNameCapacity = 64;

        /// @brief Index meaning no track has its file browser open.
        constexpr int kNoBrowsingTrack = -1;
    }

    void DrawTrackPanel(RigController& rig, std::string& track_error)
    {
        static std::array<std::array<char, kNameCapacity>, kTrackCount> names{};
        static std::string browse_directory;
        static int         browsing_track = kNoBrowsingTrack;

        SushiDSP::TrackPlayer& player = rig.player();

        for (std::size_t index = 0; index < kTrackCount; ++index)
        {
            ImGui::PushID(static_cast<int>(index));

            ImGui::SetNextItemWidth(96.0f);
            if (TextField("##name", names[index].data(), kNameCapacity, rig.track_name(index)))
            {
                rig.set_track_name(index, names[index].data());
            }

            ImGui::SameLine();
            if (ImGui::Button("Load"))
            {
                browsing_track = browsing_track == static_cast<int>(index)
                    ? kNoBrowsingTrack
                    : static_cast<int>(index);
                if (browse_directory.empty())
                {
                    browse_directory = rig.take_folder();
                }
            }

            ImGui::SameLine();
            bool muted = player.muted(index);
            if (ImGui::Checkbox("M", &muted))
            {
                player.set_mute(index, muted);
            }

            ImGui::SameLine();
            bool soloed = player.soloed(index);
            if (ImGui::Checkbox("S", &soloed))
            {
                player.set_solo(index, soloed);
            }

            ImGui::SameLine();
            const bool is_reamp_source =
                rig.reamp_track().has_value() && *rig.reamp_track() == index;
            if (ImGui::Button(is_reamp_source ? "Re-amping" : "Re-amp"))
            {
                if (is_reamp_source)
                {
                    rig.clear_reamp_track();
                    rig.set_input_source(RigInputSource::LiveInput);
                }
                else if (rig.set_reamp_track(index, track_error))
                {
                    rig.set_input_source(RigInputSource::Track);
                }
            }

            ImGui::SetNextItemWidth(110.0f);
            float level = player.gain(index);
            if (ImGui::SliderFloat("##level", &level, 0.0f, 2.0f, "%.2f"))
            {
                player.set_gain(index, level);
            }

            ImGui::SameLine();
            ImGui::SetNextItemWidth(110.0f);
            float pan = player.pan(index);
            if (ImGui::SliderFloat("##pan", &pan, -1.0f, 1.0f, "%.2f"))
            {
                player.set_pan(index, pan);
            }

            const std::string path = rig.track_path(index);
            ImGui::TextColored(to_imvec4(palette().muted_text), "%s",
                               path.empty() ? "no file" : path.c_str());

            if (browsing_track == static_cast<int>(index))
            {
                const std::optional<std::string> picked =
                    FileBrowser("##track_browser", browse_directory, ".wav");
                if (picked.has_value())
                {
                    if (rig.load_track(index, *picked, track_error))
                    {
                        track_error.clear();
                    }
                    browsing_track = kNoBrowsingTrack;
                }
            }

            ImGui::Separator();
            ImGui::PopID();
        }

        if (!track_error.empty())
        {
            ImGui::TextColored(to_imvec4(rgba(220, 72, 60)), "%s", track_error.c_str());
        }
    }
}
