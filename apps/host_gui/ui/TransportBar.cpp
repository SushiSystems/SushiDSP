/**************************************************************************/
/* TransportBar.cpp                                                       */
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

#include "ui/TransportBar.hpp"

#include <cstdint>

#include <imgui.h>

#include "ui/Theme.hpp"
#include "ui/TextField.hpp"

namespace ui
{
    namespace
    {
        /// @brief Converts a frame count to seconds at a rate.
        /// @param frames The frame count.
        /// @param sample_rate The rate in Hz; a non-positive rate reads as zero seconds.
        /// @return The time in seconds.
        float to_seconds(std::uint64_t frames, double sample_rate)
        {
            return sample_rate > 0.0
                ? static_cast<float>(static_cast<double>(frames) / sample_rate)
                : 0.0f;
        }

        /// @brief Converts a time in seconds to a frame count at a rate.
        /// @param seconds The time; a negative time reads as frame zero.
        /// @param sample_rate The rate in Hz.
        /// @return The frame count.
        std::uint64_t to_frames(float seconds, double sample_rate)
        {
            return seconds > 0.0f
                ? static_cast<std::uint64_t>(static_cast<double>(seconds) * sample_rate)
                : 0u;
        }

        /// @brief Applies the loop range the bar is holding to the transport, clearing it when
        /// the switch is off or the range is empty.
        /// @param rig The rig whose transport carries the range.
        /// @param enabled Whether the loop switch is on.
        /// @param begin_seconds The loop's begin in seconds.
        /// @param end_seconds The loop's end in seconds.
        void apply_loop(RigController& rig, bool enabled, float begin_seconds, float end_seconds)
        {
            const double rate = rig.sample_rate();
            if (!enabled || end_seconds <= begin_seconds)
            {
                rig.transport().clear_loop();
                return;
            }
            rig.transport().set_loop(to_frames(begin_seconds, rate), to_frames(end_seconds, rate));
        }
    }

    void DrawTransportBar(RigController& rig, std::string& transport_error)
    {
        static char  take_folder[256] = "";
        static char  take_name[128] = "take";
        static bool  loop_enabled = false;
        static float loop_begin_seconds = 0.0f;
        static float loop_end_seconds = 0.0f;

        rig.poll_recording(transport_error);

        SushiDSP::Transport&    transport = rig.transport();
        SushiDSP::RecordingBus& bus = rig.recording_bus();
        const bool              recording = bus.is_recording();

        ImGui::SetNextItemWidth(180.0f);
        if (TextField("##take_folder", take_folder, sizeof(take_folder), rig.take_folder()))
        {
            rig.set_take_folder(take_folder);
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        ImGui::InputText("##take_name", take_name, sizeof(take_name));
        ImGui::SameLine();

        if (ImGui::Button(recording ? "Stop Rec" : "Record"))
        {
            if (recording)
            {
                rig.stop_recording();
                transport.stop();
            }
            else
            {
                rig.arm_recording(take_name, transport_error);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Play"))
        {
            transport.play();
        }
        ImGui::SameLine();
        if (ImGui::Button("Stop"))
        {
            transport.stop();
            if (recording)
            {
                rig.stop_recording();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Rewind"))
        {
            transport.seek(0);
        }
        ImGui::SameLine();

        // The transport owns the range; the widgets only echo it until one of them is turned.
        if (!ImGui::IsAnyItemActive())
        {
            loop_enabled = transport.has_loop();
            if (loop_enabled)
            {
                loop_begin_seconds = to_seconds(transport.loop_begin_frames(), rig.sample_rate());
                loop_end_seconds = to_seconds(transport.loop_end_frames(), rig.sample_rate());
            }
        }

        bool loop_changed = ImGui::Checkbox("Loop", &loop_enabled);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(70.0f);
        loop_changed |= ImGui::InputFloat("##loop_begin", &loop_begin_seconds, 0.0f, 0.0f, "%.2f");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(70.0f);
        loop_changed |= ImGui::InputFloat("##loop_end", &loop_end_seconds, 0.0f, 0.0f, "%.2f");
        if (loop_changed)
        {
            apply_loop(rig, loop_enabled, loop_begin_seconds, loop_end_seconds);
        }
        ImGui::SameLine();

        int source = static_cast<int>(rig.input_source());
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::Combo("##rig_input", &source, "Live\0Track\0Live + Track\0"))
        {
            rig.set_input_source(static_cast<RigInputSource>(source));
        }
        ImGui::SameLine();

        ImGui::TextColored(to_imvec4(palette().muted_text), "%.2f s",
                           static_cast<double>(to_seconds(transport.position_frames(),
                                                          rig.sample_rate())));

        const std::uint64_t dropped = bus.dropped_blocks();
        if (dropped != 0)
        {
            ImGui::SameLine();
            ImGui::TextColored(to_imvec4(rgba(220, 72, 60)), "dropped %llu",
                               static_cast<unsigned long long>(dropped));
        }

        if (!transport_error.empty())
        {
            ImGui::SameLine();
            ImGui::TextColored(to_imvec4(rgba(220, 72, 60)), "%s", transport_error.c_str());
        }
    }
}
