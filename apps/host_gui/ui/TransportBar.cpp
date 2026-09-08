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

#include <algorithm>
#include <cstdint>
#include <optional>

#include <imgui.h>

#include "ui/MusicalTimeText.hpp"
#include "ui/TapTempo.hpp"
#include "ui/TextField.hpp"
#include "ui/Theme.hpp"

namespace ui
{
    namespace
    {
        /// @brief The slowest tempo the BPM box accepts, in quarter notes per minute.
        constexpr float kMinimumTempo = 30.0f;

        /// @brief The fastest tempo the BPM box accepts, in quarter notes per minute.
        constexpr float kMaximumTempo = 300.0f;

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

        /// @brief Converts a bar number to the frame that bar's first beat starts on.
        /// @param bar The bar, counted from 1; a bar below 1 is read as 1.
        /// @param clock The grid the bar belongs to.
        /// @return The frame.
        std::uint64_t frame_of_bar(int bar, const SushiDSP::MusicalClock& clock)
        {
            return clock.frame_of(bar > 1 ? static_cast<std::uint64_t>(bar) : 1u, 1u);
        }

        /// @brief Names the bar a frame falls in, for the loop's two boxes.
        /// @param frame The frame.
        /// @param clock The grid the bar is read from.
        /// @return The bar, counted from 1.
        int bar_of_frame(std::uint64_t frame, const SushiDSP::MusicalClock& clock)
        {
            return static_cast<int>(clock.position_of(frame).bar);
        }

        /// @brief Applies the loop range the bar is holding to the transport, clearing it when
        /// the switch is off or the range is empty. The range is typed in bars and stored in
        /// frames, so a later tempo change leaves the audio it loops over where it was.
        /// @param rig The rig whose transport carries the range.
        /// @param enabled Whether the loop switch is on.
        /// @param begin_bar The loop's first bar.
        /// @param end_bar The bar the loop returns at.
        void apply_loop(RigController& rig, bool enabled, int begin_bar, int end_bar)
        {
            const SushiDSP::MusicalClock& clock = rig.musical_clock();

            if (!enabled || end_bar <= begin_bar)
            {
                rig.transport().clear_loop();
                return;
            }

            rig.transport().set_loop(frame_of_bar(begin_bar, clock), frame_of_bar(end_bar, clock));
        }

        /// @brief Draws the take folder and name boxes and the record, play, stop and rewind
        /// buttons.
        /// @param rig The rig whose recorder and transport the buttons drive.
        /// @param take_name The take name box's buffer.
        /// @param take_name_capacity How many characters that buffer holds.
        /// @param error_out In/out: the last refusal.
        void draw_take_and_buttons(RigController& rig, char* take_name,
                                   std::size_t take_name_capacity, std::string& error_out)
        {
            static char take_folder[256] = "";

            SushiDSP::Transport& transport = rig.transport();
            const bool           recording = rig.recording_bus().is_recording();

            ImGui::SetNextItemWidth(150.0f);
            if (TextField("##take_folder", take_folder, sizeof(take_folder), rig.take_folder()))
            {
                rig.set_take_folder(take_folder);
            }

            ImGui::SameLine();
            ImGui::SetNextItemWidth(86.0f);
            ImGui::InputText("##take_name", take_name, take_name_capacity);

            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button,
                                  to_imvec4(recording ? rgba(196, 54, 46) : rgba(52, 52, 60)));
            if (ImGui::Button(recording ? "STOP REC" : "REC"))
            {
                if (recording)
                {
                    rig.stop_recording();
                    transport.stop();
                }
                else
                {
                    rig.arm_recording(take_name, error_out);
                }
            }
            ImGui::PopStyleColor();

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
            if (ImGui::Button("|<"))
            {
                transport.seek(0);
            }
        }

        /// @brief Draws the loop switch and the two bar numbers that bound it.
        /// @param rig The rig whose transport carries the range.
        void draw_loop(RigController& rig)
        {
            static bool loop_enabled = false;
            static int  loop_begin_bar = 1;
            static int  loop_end_bar = 5;

            const SushiDSP::Transport& transport = rig.transport();

            // The transport owns the range; the widgets only echo it until one of them is turned.
            if (!ImGui::IsAnyItemActive())
            {
                loop_enabled = transport.has_loop();
                if (loop_enabled)
                {
                    loop_begin_bar = bar_of_frame(transport.loop_begin_frames(),
                                                  rig.musical_clock());
                    loop_end_bar = bar_of_frame(transport.loop_end_frames(), rig.musical_clock());
                }
            }

            bool changed = ImGui::Checkbox("Loop", &loop_enabled);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(52.0f);
            changed |= ImGui::InputInt("##loop_begin", &loop_begin_bar, 0, 0);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(52.0f);
            changed |= ImGui::InputInt("##loop_end", &loop_end_bar, 0, 0);

            if (changed)
            {
                apply_loop(rig, loop_enabled, loop_begin_bar, loop_end_bar);
            }
        }

        /// @brief Draws the tempo controls: the BPM box, the meter's two numbers and the TAP
        /// button.
        /// @param rig The rig whose musical clock the controls write.
        void draw_tempo(RigController& rig)
        {
            static TapTempo taps;

            SushiDSP::MusicalClock& clock = rig.musical_clock();

            ImGui::SetNextItemWidth(66.0f);
            float tempo = static_cast<float>(clock.beats_per_minute());
            if (ImGui::DragFloat("##bpm", &tempo, 0.25f, kMinimumTempo, kMaximumTempo, "%.1f BPM"))
            {
                clock.set_tempo(static_cast<double>(tempo));
                // A tempo typed by hand ends whatever tap run was in progress, so the next tap
                // opens a run of its own rather than averaging across the change.
                taps.reset();
            }

            ImGui::SameLine();
            if (ImGui::Button("TAP"))
            {
                const std::optional<double> tapped = taps.tap(ImGui::GetTime());
                if (tapped.has_value())
                {
                    clock.set_tempo(*tapped);
                }
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Tap the beat; the last %zu taps set the tempo.",
                                  TapTempo::kTapCount);
            }

            ImGui::SameLine();
            ImGui::SetNextItemWidth(38.0f);
            int beats_per_bar = static_cast<int>(clock.beats_per_bar());
            int beat_unit = static_cast<int>(clock.beat_unit());
            bool meter_changed = ImGui::InputInt("##beats_per_bar", &beats_per_bar, 0, 0);
            ImGui::SameLine(0.0f, 4.0f);
            ImGui::TextColored(to_imvec4(palette().muted_text), "/");
            ImGui::SameLine(0.0f, 4.0f);
            ImGui::SetNextItemWidth(38.0f);
            meter_changed |= ImGui::InputInt("##beat_unit", &beat_unit, 0, 0);

            if (meter_changed)
            {
                clock.set_meter(static_cast<std::uint32_t>(std::max(beats_per_bar, 1)),
                                static_cast<std::uint32_t>(std::max(beat_unit, 1)));
            }
        }

        /// @brief Draws the metronome's switch and the level beside it, which is where spec §10's
        /// eighth ruling puts them.
        /// @param rig The rig whose metronome the two controls drive.
        void draw_metronome(RigController& rig)
        {
            SushiDSP::Metronome& metronome = rig.metronome();

            bool enabled = metronome.enabled();
            ImGui::PushStyleColor(ImGuiCol_Button,
                                  to_imvec4(enabled ? rgba(64, 152, 196) : rgba(52, 52, 60)));
            if (ImGui::Button("Click"))
            {
                metronome.set_enabled(!enabled);
            }
            ImGui::PopStyleColor();

            ImGui::SameLine();
            ImGui::SetNextItemWidth(72.0f);
            float level = metronome.level();
            if (ImGui::SliderFloat("##click_level", &level, 0.0f, 1.0f, "%.2f"))
            {
                metronome.set_level(level);
            }
        }

        /// @brief Draws the playback speed slider. Slowing the transport drops the pitch with
        /// it, tape-style, which is what the label says and what spec §5a specifies; nothing
        /// here stretches time.
        /// @param rig The rig whose transport carries the speed.
        void draw_speed(RigController& rig)
        {
            SushiDSP::Transport& transport = rig.transport();

            ImGui::SetNextItemWidth(120.0f);
            float speed = static_cast<float>(transport.speed());
            if (ImGui::SliderFloat("##speed", &speed,
                                   static_cast<float>(SushiDSP::Transport::kMinSpeed),
                                   static_cast<float>(SushiDSP::Transport::kMaxSpeed),
                                   "%.2fx tape"))
            {
                transport.set_speed(static_cast<double>(speed));
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Playback speed. The pitch falls with it, as a tape machine's "
                                  "does: half speed is an octave down.");
            }

            // The slider consumes its own clicks, so the way back to unity is a button beside it
            // rather than a double-click the slider would have swallowed.
            ImGui::SameLine(0.0f, 4.0f);
            if (ImGui::SmallButton("1x"))
            {
                transport.set_speed(1.0);
            }
        }
    }

    void DrawTransportBar(RigController& rig, std::string& transport_error)
    {
        static char take_name[128] = "take";

        rig.poll_recording(transport_error);

        draw_take_and_buttons(rig, take_name, sizeof(take_name), transport_error);

        ImGui::SameLine();
        draw_loop(rig);

        ImGui::SameLine();
        int source = static_cast<int>(rig.input_source());
        ImGui::SetNextItemWidth(96.0f);
        if (ImGui::Combo("##rig_input", &source, "Live\0Track\0Live + Track\0"))
        {
            rig.set_input_source(static_cast<RigInputSource>(source));
        }

        const std::uint64_t position = rig.transport().position_frames();
        ImGui::SameLine();
        ImGui::TextColored(to_imvec4(palette().text), "%s",
                           bar_beat_sixteenth_text(position, rig.musical_clock()).c_str());
        ImGui::SameLine();
        ImGui::TextColored(to_imvec4(palette().muted_text), "%.2f s",
                           static_cast<double>(to_seconds(position, rig.sample_rate())));

        // The second row exists because the first outgrew the window: the controls below need
        // about 1250 pixels beside the ones above, against a 1136-wide reference window.
        draw_metronome(rig);

        ImGui::SameLine();
        draw_tempo(rig);

        ImGui::SameLine();
        draw_speed(rig);

        const std::uint64_t dropped = rig.recording_bus().dropped_blocks();
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
