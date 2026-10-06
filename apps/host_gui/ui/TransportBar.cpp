/**************************************************************************/
/* TransportBar.cpp                                                       */
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
 * @file TransportBar.cpp
 * @brief Implements the bottom transport bar.
 * @author Mustafa Garip
 */

#include "ui/TransportBar.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>

#include <imgui.h>

#include "ui/MeterBar.hpp"
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

        /// @brief The loop switch and the range it bounds, echoed from the transport until a
        /// widget is turned; shared between the switch (main row) and the range boxes (the
        /// wrapping row of controls the mockup does not show).
        struct LoopState
        {
            bool enabled = false;
            int  begin_bar = 1;
            int  end_bar = 5;
        };

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
        /// @param loop The switch and the range it bounds.
        void apply_loop(RigController& rig, const LoopState& loop)
        {
            const SushiDSP::MusicalClock& clock = rig.musical_clock();

            if (!loop.enabled || loop.end_bar <= loop.begin_bar)
            {
                rig.transport().clear_loop();
                return;
            }

            rig.transport().set_loop(frame_of_bar(loop.begin_bar, clock),
                                     frame_of_bar(loop.end_bar, clock));
        }

        /// @brief Refreshes the echoed loop state from the transport, unless a loop widget is
        /// the one currently being edited.
        /// @param rig The rig whose transport carries the range.
        /// @param loop In/out: the echoed switch and range.
        void sync_loop_state(RigController& rig, LoopState& loop)
        {
            if (ImGui::IsAnyItemActive())
            {
                return;
            }

            const SushiDSP::Transport& transport = rig.transport();
            loop.enabled = transport.has_loop();
            if (loop.enabled)
            {
                loop.begin_bar = bar_of_frame(transport.loop_begin_frames(), rig.musical_clock());
                loop.end_bar = bar_of_frame(transport.loop_end_frames(), rig.musical_clock());
            }
        }

        /// @brief Draws the record, play, stop and rewind buttons.
        /// @param rig The rig whose recorder and transport the buttons drive.
        /// @param take_name The take name the record button arms with.
        /// @param error_out In/out: the last refusal.
        void draw_transport_buttons(RigController& rig, const char* take_name,
                                    std::string& error_out)
        {
            SushiDSP::Transport& transport = rig.transport();
            const bool           recording = rig.recording_bus().is_recording();

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

        /// @brief Draws the loop switch alone; the range it bounds is drawn separately.
        /// @param rig The rig whose transport carries the range.
        /// @param loop In/out: the echoed switch and range.
        void draw_loop_toggle(RigController& rig, LoopState& loop)
        {
            if (ImGui::Checkbox("Loop", &loop.enabled))
            {
                apply_loop(rig, loop);
            }
        }

        /// @brief Draws the loop's two bar-number boxes; not in the mockup, kept in the wrapping
        /// row of controls it does not show.
        /// @param rig The rig whose transport carries the range.
        /// @param loop In/out: the echoed switch and range.
        void draw_loop_range(RigController& rig, LoopState& loop)
        {
            bool changed = false;

            ImGui::SetNextItemWidth(52.0f);
            changed |= ImGui::InputInt("##loop_begin", &loop.begin_bar, 0, 0);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(52.0f);
            changed |= ImGui::InputInt("##loop_end", &loop.end_bar, 0, 0);

            if (changed)
            {
                apply_loop(rig, loop);
            }
        }

        /// @brief Draws the take folder and take name boxes; not in the mockup, kept in the
        /// wrapping row of controls it does not show.
        /// @param rig The rig whose take folder the box writes.
        /// @param take_name The take name box's buffer.
        /// @param take_name_capacity How many characters that buffer holds.
        void draw_take_fields(RigController& rig, char* take_name,
                              std::size_t take_name_capacity)
        {
            static char take_folder[256] = "";

            ImGui::SetNextItemWidth(150.0f);
            if (TextField("##take_folder", take_folder, sizeof(take_folder), rig.take_folder()))
            {
                rig.set_take_folder(take_folder);
            }

            ImGui::SameLine();
            ImGui::SetNextItemWidth(86.0f);
            ImGui::InputText("##take_name", take_name, take_name_capacity);
        }

        /// @brief Draws the rig's input source combo; not in the mockup, kept in the wrapping
        /// row of controls it does not show.
        /// @param rig The rig whose input source the combo writes.
        void draw_input_source(RigController& rig)
        {
            int source = static_cast<int>(rig.input_source());
            ImGui::SetNextItemWidth(96.0f);
            if (ImGui::Combo("##rig_input", &source, "Live\0Track\0Live + Track\0"))
            {
                rig.set_input_source(static_cast<RigInputSource>(source));
            }
        }

        /// @brief Draws the transport position, in bars-beats-sixteenths and in seconds.
        /// @param rig The rig whose transport and musical clock the readout follows.
        void draw_position(RigController& rig)
        {
            const std::uint64_t position = rig.transport().position_frames();
            ImGui::TextColored(to_imvec4(palette().text), "%s",
                               bar_beat_sixteenth_text(position, rig.musical_clock()).c_str());
            ImGui::SameLine();
            ImGui::TextColored(to_imvec4(palette().muted_text), "%.2f s",
                               static_cast<double>(to_seconds(position, rig.sample_rate())));
        }

        /// @brief Draws the metronome's switch alone; its level is not in the mockup and stays in
        /// the wrapping row of controls it does not show.
        /// @param rig The rig whose metronome the switch drives.
        void draw_metronome_switch(RigController& rig)
        {
            SushiDSP::Metronome& metronome = rig.metronome();

            const bool enabled = metronome.enabled();
            ImGui::PushStyleColor(ImGuiCol_Button,
                                  to_imvec4(enabled ? rgba(64, 152, 196) : rgba(52, 52, 60)));
            if (ImGui::Button("Click"))
            {
                metronome.set_enabled(!enabled);
            }
            ImGui::PopStyleColor();
        }

        /// @brief Draws the metronome's level slider; not in the mockup, kept in the wrapping row
        /// of controls it does not show.
        /// @param rig The rig whose metronome the slider drives.
        void draw_metronome_level(RigController& rig)
        {
            SushiDSP::Metronome& metronome = rig.metronome();

            ImGui::SetNextItemWidth(72.0f);
            float level = metronome.level();
            if (ImGui::SliderFloat("##click_level", &level, 0.0f, 1.0f, "%.2f"))
            {
                metronome.set_level(level);
            }
        }

        /// @brief Draws the BPM box.
        /// @param rig The rig whose musical clock the box writes.
        /// @param taps The running tap-tempo average the BPM box resets on a manual edit.
        void draw_tempo_bpm(RigController& rig, TapTempo& taps)
        {
            SushiDSP::MusicalClock& clock = rig.musical_clock();

            ImGui::SetNextItemWidth(66.0f);
            float tempo = static_cast<float>(clock.beats_per_minute());
            if (ImGui::DragFloat("##bpm", &tempo, 0.25f, kMinimumTempo, kMaximumTempo, "%.1f BPM"))
            {
                clock.set_tempo(static_cast<double>(tempo));
                // A tempo typed by hand ends the tap run, so the next tap opens a run of its own.
                taps.reset();
            }
        }

        /// @brief Draws the time signature's two numbers.
        /// @param rig The rig whose musical clock the boxes write.
        void draw_time_signature(RigController& rig)
        {
            SushiDSP::MusicalClock& clock = rig.musical_clock();

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

        /// @brief Draws the TAP button.
        /// @param rig The rig whose musical clock a tapped tempo writes.
        /// @param taps The running tap-tempo average.
        void draw_tap(RigController& rig, TapTempo& taps)
        {
            if (ImGui::Button("Tap"))
            {
                const std::optional<double> tapped = taps.tap(ImGui::GetTime());
                if (tapped.has_value())
                {
                    rig.musical_clock().set_tempo(*tapped);
                }
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Tap the beat; the last %zu taps set the tempo.",
                                  TapTempo::kTapCount);
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

            // The slider consumes its own clicks, so a button beside it is the way back to unity.
            ImGui::SameLine(0.0f, 4.0f);
            if (ImGui::SmallButton("1x"))
            {
                transport.set_speed(1.0);
            }
        }

        /// @brief Draws the disabled Pitch field the track pitch-shift plan will wire up.
        void draw_pitch()
        {
            char pitch[8] = "0 st";
            ImGui::BeginDisabled();
            ImGui::SetNextItemWidth(52.0f);
            ImGui::InputText("##pitch", pitch, sizeof(pitch), ImGuiInputTextFlags_ReadOnly);
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Pitch shift arrives with the track pitch-shift plan");
            }
        }

        /// @brief Draws the Mini-DAW / Back to Rig toggle button.
        /// @param mini_daw_active In/out: whether the workspace shows the mini-DAW.
        void draw_mini_daw_toggle(bool& mini_daw_active)
        {
            if (ImGui::Button(mini_daw_active ? "Back to Rig" : "Mini-DAW"))
            {
                mini_daw_active = !mini_daw_active;
            }
        }
    }

    void DrawTransportBar(RigController& rig, LevelProbe& input, LevelProbe& output,
                          bool& mini_daw_active, std::string& transport_error)
    {
        static char take_name[128] = "take";
        static LoopState loop;
        static TapTempo taps;

        rig.poll_recording(transport_error);
        sync_loop_state(rig, loop);

        // Main row: exactly the mockup's order (§12).
        draw_transport_buttons(rig, take_name, transport_error);
        ImGui::SameLine();
        draw_loop_toggle(rig, loop);
        ImGui::SameLine();
        draw_position(rig);
        ImGui::SameLine();
        draw_metronome_switch(rig);
        ImGui::SameLine();
        draw_tempo_bpm(rig, taps);
        ImGui::SameLine();
        draw_time_signature(rig);
        ImGui::SameLine();
        draw_tap(rig, taps);
        ImGui::SameLine();
        draw_speed(rig);
        ImGui::SameLine();
        draw_pitch();
        ImGui::SameLine();
        draw_mini_daw_toggle(mini_daw_active);

        // Meters, pinned to the right edge.
        ImGui::SameLine(ImGui::GetWindowWidth() - 220.0f);
        DrawInlineMeters(input, output);

        // Second, wrapping row: everything the mockup does not show.
        draw_take_fields(rig, take_name, sizeof(take_name));
        ImGui::SameLine();
        draw_loop_range(rig, loop);
        ImGui::SameLine();
        draw_input_source(rig);
        ImGui::SameLine();
        draw_metronome_level(rig);

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
