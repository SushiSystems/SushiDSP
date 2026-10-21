/**************************************************************************/
/* main.cpp                                                               */
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
 * @file main.cpp
 * @brief Runs the host's window: opens the audio device, builds the rig and draws each frame.
 * @author Mustafa Garip
 */

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include <SushiDSP/foundation/core/audio_device.hpp>
#include <SushiDSP/foundation/core/rig_graph.hpp>
#include <SushiDSP/foundation/core/rt_graph_slot.hpp>
#include <SushiDSP/foundation/audio_format/channel_selection.hpp>
#include <SushiDSP/foundation/device/sdl2_audio_device.hpp>

#include "audio_import_cache.hpp"
#include "audio_import_queue.hpp"
#include "file_drop_queue.hpp"
#include "rig_controller.hpp"
#include "rig_topology.hpp"
#include "topology_applier.hpp"
#include "ui/arrangement_layout.hpp"
#include "ui/arrangement_view.hpp"
#include "ui/device_settings.hpp"
#include "ui/divider.hpp"
#include "ui/divider_math.hpp"
#include "ui/gear_browser.hpp"
#include "ui/level_probe.hpp"
#include "ui/meter_bar.hpp"
#include "ui/cabinet_section_pane.hpp"
#include "ui/rig_chain_view.hpp"
#include "ui/room_panel.hpp"
#include "ui/stage_panel.hpp"
#include "ui/theme.hpp"
#include "ui/top_bar.hpp"
#include "session.hpp"
#include "session_bounce.hpp"
#include "session_file.hpp"
#include "session_renderer.hpp"
#include "ui/transport_bar.hpp"

#if defined(SUSHIDSP_HAVE_ASIO)
#include <SushiDSP/foundation/device/asio_audio_device.hpp>
#endif

namespace
{
    /// @brief Derives the directory the running executable sits in from argv[0].
    /// @param argv0 The process' argv[0].
    /// @return The directory, with a trailing separator, or "" if argv[0] carries no path.
    std::string executable_directory(const char* argv0)
    {
        const std::string path = argv0 != nullptr ? std::string(argv0) : std::string();
        const std::size_t cut = path.find_last_of("/\\");
        return cut == std::string::npos ? std::string() : path.substr(0, cut + 1);
    }

    /// @brief Queues every path Windows Explorer drops on the window, with the cursor position
    /// it landed at, for the next GUI frame to route onto a lane. GLFW calls this outside the
    /// ImGui frame, which is why the queue exists at all.
    /// @param window The window the files were dropped on; its user pointer is the queue.
    /// @param count How many paths were dropped.
    /// @param paths The dropped paths, owned by GLFW for the duration of the call.
    /// @see the arrangement view design spec in the version history §7
    void queue_dropped_files(GLFWwindow* window, int count, const char** paths)
    {
        auto* queue = static_cast<SushiDSP::host::FileDropQueue*>(glfwGetWindowUserPointer(window));
        if (queue == nullptr)
        {
            return;
        }

        double cursor_x = 0.0;
        double cursor_y = 0.0;
        glfwGetCursorPos(window, &cursor_x, &cursor_y);

        // The layout works in framebuffer pixels; glfwGetCursorPos reports window coordinates.
        int window_width = 0;
        int window_height = 0;
        int framebuffer_width = 0;
        int framebuffer_height = 0;
        glfwGetWindowSize(window, &window_width, &window_height);
        glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);
        if (window_width > 0 && window_height > 0)
        {
            cursor_x *= static_cast<double>(framebuffer_width) / static_cast<double>(window_width);
            cursor_y *=
                static_cast<double>(framebuffer_height) / static_cast<double>(window_height);
        }

        for (int index = 0; index < count; ++index)
        {
            queue->push(paths[index], cursor_x, cursor_y);
        }
    }

    /// @brief Installs the bundled HRTF set into a fresh rig, so every room the rig builds from
    /// then on takes it; without the file the rooms keep their spherical-head fallback and one
    /// line on stderr names the path that was looked for. See assets/hrtf/README.md.
    /// @param controller The rig to act on.
    /// @param default_hrtf_path Path to the bundled set.
    void apply_default_hrtf(Session& session, const std::string& default_hrtf_path)
    {
        std::string error;
        if (session.set_hrtf_path(default_hrtf_path, error))
        {
            return;
        }
        std::fprintf(
            stderr,
            "No bundled HRTF set: %s - rooms use the spherical-head fallback, so front and back "
            "sound alike. See assets/hrtf/README.md.\n",
            error.c_str());
    }

    /// @brief Installs the bundled cabinet impulse response on a freshly-created slot that hosts
    /// a cabinet, if the bundle is present next to the executable; without it the slot starts
    /// with no cabinet, which is a valid state, and nothing is printed.
    /// @param controller The rig to act on.
    /// @param slot The new slot's id.
    /// @param default_ir_path Path to the bundled impulse response.
    void apply_default_cabinet(RigController& controller, RigController::SlotId slot,
                               const std::string& default_ir_path)
    {
        if (!controller.hosts_cabinet(slot))
        {
            return;
        }

        std::ifstream probe(default_ir_path, std::ios::binary);
        if (!probe)
        {
            return;
        }
        probe.close();

        try
        {
            controller.set_cabinet(slot, default_ir_path);
        }
        catch (const std::exception& error)
        {
            std::fprintf(stderr, "Bundled cabinet impulse response failed to load: %s\n",
                         error.what());
        }
    }

    /// @brief Resets the chain view's state to the rig the controller holds and lays it out.
    ///
    /// A rig that carries its shape, which every preset this build writes does, comes back with
    /// its stages, its DI choice and its hidden slots. Any other rig is taken as one path in slot
    /// order; a room such a preset carried is moved into the cabinet section.
    /// @param rig The rig the state now describes.
    /// @param state The chain view state to reset.
    /// @param status Out: a message when the layout was refused.
    void seed_rig_chain(RigController& rig, ui::RigChainState& state, std::string& status)
    {
        state.applier = TopologyApplier();
        state.selected = std::nullopt;
        state.edit_error.clear();
        if (rig.shape().has_value())
        {
            const RigShape shape = *rig.shape();
            state.topology = RigTopology::from_stages(shape.chain_type, shape.branches, shape.post,
                                                      shape.di, shape.di_position);
            state.applier.adopt(shape.hidden, rig);
        }
        else
        {
            state.topology = RigTopology::from_linear_order(rig.slot_order());
        }
        try
        {
            state.applier.apply(state.topology, rig);
            if (const std::optional<LegacyRoom> legacy = rig.take_legacy_room())
            {
                state.applier.migrate_legacy_room(state.topology, rig, *legacy);
                status = "The preset's room was moved into the cabinet section.";
            }
        }
        catch (const std::exception& error)
        {
            status = error.what();
        }
    }
} // namespace

int main(int argc, char** argv)
{
    const std::string default_ir_directory =
        executable_directory(argc > 0 ? argv[0] : nullptr) + "assets/ir";
    const std::string default_ir_path = default_ir_directory + "/default_4x12.wav";
    const std::string default_hrtf_path =
        executable_directory(argc > 0 ? argv[0] : nullptr) + "assets/hrtf/sadie2_ku100_48k.hrtf";

    if (!glfwInit())
    {
        std::fprintf(stderr, "glfwInit failed\n");
        return 1;
    }

    GLFWwindow* window = glfwCreateWindow(1280, 800, "SushiDSP Rig", nullptr, nullptr);
    if (window == nullptr)
    {
        std::fprintf(stderr, "glfwCreateWindow failed\n");
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    SushiDSP::host::FileDropQueue file_drops;
    glfwSetWindowUserPointer(window, &file_drops);
    glfwSetDropCallback(window, queue_dropped_files);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ui::apply_theme();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    const std::vector<std::string> asio_drivers =
#if defined(SUSHIDSP_HAVE_ASIO)
        SushiDSP::AsioAudioDevice::enumerate_drivers();
#else
        std::vector<std::string>{};
#endif

    ui::DeviceChoice device_choice{0, SushiDSP::ChannelSelection::Left,
                                   SushiDSP::ChannelSelection::Stereo};
    ui::RigChainState rig_chain_state;
    std::string device_label = "not connected";
    std::string status;
    std::string connect_error;
    std::string transport_error;
    std::string arrangement_error;
    std::string room_error;
    ui::CabinetSectionState cabinet_section_state;
    // The workspace shows the rig or the cabinet section; the choice is view state.
    bool cabinet_section_open = false;
    bool open_device_settings = true;
    // The mini-DAW (arrangement view + meter bar) starts hidden.
    bool mini_daw_open = false;

    // The zoom, the scroll and the snap are view state, so the GUI loop owns them, not the rig.
    ui::TimelineScale timeline_scale;
    ui::SnapMode snap_mode = ui::SnapMode::Bar;
    ui::LaneGeometry lane_geometry;
    float arrangement_header_width = 260.0f;

    // Pane sizes the dividers drag; each is re-fitted to the window every frame.
    float bottom_bar_height = 96.0f;
    float settings_height = 430.0f;
    float gear_width = 300.0f;

    // Sized once by the first Explorer drop and reused, so a drop costs no allocation after it.
    std::vector<SushiDSP::host::FileDrop> drained_drops;

    // A dropped mp3 is decoded to a WAV beside the executable; see applications/host_gui/README.md.
    SushiDSP::host::AudioImportQueue audio_imports{SushiDSP::host::AudioImportCache(
        executable_directory(argc > 0 ? argv[0] : nullptr) + "cache/imported_audio")};
    std::vector<SushiDSP::host::AudioImport> finished_imports;

    // The audio callback captures these by reference; applications/host_gui/README.md states the lifetime.
    std::vector<float> rig_input;
    // The second host channel the rig reads; a Parallel chain takes its right path from it.
    std::vector<float> rig_input_second;
    std::vector<float> rig_output;
    std::vector<float*> rig_output_pointers;
    std::vector<const float*> rig_output_const_pointers;
    std::vector<float> callback_silence;
    ui::LevelProbe input_probe;
    ui::LevelProbe output_probe;

    // session_storage outlives device by declaration order; see applications/host_gui/README.md.
    std::optional<Session> session_storage;
    SessionRenderer session_renderer;
    SessionBounce session_bounce;
    // The rig view is laid out again whenever the session shows it another rig.
    std::uint64_t seeded_rig_revision = ~std::uint64_t{0};
    std::unique_ptr<SushiDSP::AudioDevice> device;

    /// @brief Opens the chosen device, builds a rig for its format and starts the audio callback.
    /// Records the failure in connect_error and leaves the previous rig gone on any exception.
    auto connect_device = [&device, &device_choice, &asio_drivers, &connect_error,
                           &session_storage, &session_renderer, &rig_input, &rig_input_second, &rig_output, &rig_output_pointers,
                           &rig_output_const_pointers, &callback_silence, &input_probe,
                           &output_probe, &device_label, &rig_chain_state, &status,
                           &open_device_settings, &default_hrtf_path]()
    {
        connect_error.clear();

        // The previous device is destroyed before the next is built; see applications/host_gui/README.md.
        device.reset();

        try
        {
            if (device_choice.device_index == 0)
            {
                SushiDSP::DeviceFormat requested{48000.0, 256, 1, 2};
                device = std::make_unique<SushiDSP::Sdl2AudioDevice>(requested);
            }
#if defined(SUSHIDSP_HAVE_ASIO)
            else
            {
                SushiDSP::DeviceFormat requested{48000.0, 256, 2, 2};
                device = std::make_unique<SushiDSP::AsioAudioDevice>(
                    asio_drivers[static_cast<std::size_t>(device_choice.device_index - 1)],
                    requested);
            }
#endif

            const SushiDSP::DeviceFormat actual = device->format();
            // The rig takes one input and up to K_MAX_CHANNELS outputs; see applications/host_gui/README.md.
            const std::uint32_t rig_channels = std::min<std::uint32_t>(
                std::max(1u, actual.output_channels), SushiDSP::RigGraph::K_MAX_CHANNELS);
            SushiDSP::PrepareInfo info{actual.sample_rate, actual.block_size, rig_channels};
            session_storage.emplace(info);
            session_renderer.prepare(actual.block_size);
            apply_default_hrtf(*session_storage, default_hrtf_path);
            seed_rig_chain(session_storage->selected_rig(), rig_chain_state, status);

            const std::size_t input_index =
                SushiDSP::channel_selection_input_index(device_choice.input);
            const SushiDSP::ChannelSelection output_selection = device_choice.output;
            const bool is_multi_channel_input = actual.input_channels >= 2;
            const std::uint32_t device_output_channels = std::max(1u, actual.output_channels);
            if (!SushiDSP::rig_output_adapts_to_device(rig_channels, device_output_channels))
            {
                throw std::runtime_error(
                    "no channel adaptation rule from the rig's " + std::to_string(rig_channels) +
                    " output channels to the device's " + std::to_string(device_output_channels));
            }

            // The buffers are sized once here and never resized; see applications/host_gui/README.md.
            rig_input.assign(actual.block_size, 0.0f);
            rig_input_second.assign(actual.block_size, 0.0f);
            // The session renders two channels whatever the device takes of them.
            const std::uint32_t mix_channels = SessionRenderer::K_CHANNELS;
            rig_output.assign(static_cast<std::size_t>(actual.block_size) * mix_channels, 0.0f);
            rig_output_pointers.resize(mix_channels);
            rig_output_const_pointers.resize(mix_channels);
            for (std::uint32_t channel = 0; channel < mix_channels; ++channel)
            {
                float* start =
                    rig_output.data() + static_cast<std::size_t>(channel) * actual.block_size;
                rig_output_pointers[channel] = start;
                rig_output_const_pointers[channel] = start;
            }
            callback_silence.assign(actual.block_size, 0.0f);

            Session& session_ref = *session_storage;
            device->start(
                [&session_ref, &session_renderer, &rig_input, &rig_output_pointers,
                 &rig_output_const_pointers, &callback_silence, &input_probe, &output_probe,
                 input_index, output_selection, is_multi_channel_input, device_output_channels,
                 rig_channels](const float* const* inputs, float* const* outputs,
                               std::uint32_t frames)
                {
                    if (frames > rig_input.size())
                    {
                        return;
                    }
                    const float* selected_channel[1];
                    if (inputs == nullptr)
                    {
                        const float* silence_channel = callback_silence.data();
                        selected_channel[0] = silence_channel;
                    }
                    else if (is_multi_channel_input)
                    {
                        selected_channel[0] = inputs[input_index];
                    }
                    else
                    {
                        selected_channel[0] = inputs[0];
                    }

                    input_probe.report(selected_channel[0], frames);

                    // A mono device repeats its channel; a stereo one gives the other input.
                    const float* second_channel = selected_channel[0];
                    if (is_multi_channel_input && inputs != nullptr)
                    {
                        second_channel = inputs[input_index == 0 ? 1u : 0u];
                    }
                    const float* const live[SessionRenderer::K_CHANNELS] = {selected_channel[0],
                                                                           second_channel};
                    session_renderer.render(session_ref, live, rig_output_pointers.data(), frames);
                    output_probe.report(rig_output_pointers[0], frames);

                    SushiDSP::write_rig_output_to_device(
                        rig_output_const_pointers.data(), rig_channels, outputs,
                        device_output_channels, frames, output_selection);
                });

            device_label =
                std::string("Connected: ") +
                (device_choice.device_index == 0
                     ? "SDL2"
                     : asio_drivers[static_cast<std::size_t>(device_choice.device_index - 1)]) +
                " (input " + SushiDSP::channel_selection_label(device_choice.input) + ", output " +
                SushiDSP::channel_selection_label(device_choice.output) + ")";
            open_device_settings = false;
        }
        catch (const std::exception& error)
        {
            connect_error = error.what();
        }
    };

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        if (session_storage.has_value())
        {
            session_storage->reclaim();
            if (session_storage->rig_revision() != seeded_rig_revision)
            {
                // Another rig is on show: every slot id the views hold belongs to the last one.
                seed_rig_chain(session_storage->selected_rig(), rig_chain_state, status);
                cabinet_section_state = ui::CabinetSectionState();
                room_error.clear();
                seeded_rig_revision = session_storage->rig_revision();
            }
        }

        // Drops resolve against the previous frame's lane rectangles; see applications/host_gui/README.md.
        file_drops.drain(drained_drops);
        for (const SushiDSP::host::FileDrop& drop : drained_drops)
        {
            if (!session_storage.has_value())
            {
                continue;
            }

            const float screen_x = static_cast<float>(drop.screen_x);
            const float screen_y = static_cast<float>(drop.screen_y);
            const std::optional<std::size_t> lane = ui::lane_at(lane_geometry, screen_x, screen_y);
            if (!lane.has_value())
            {
                arrangement_error = "Dropped file landed outside the lanes: drop it on a lane.";
                continue;
            }

            const ImVec2 landed(ui::canvas_x_of(lane_geometry, screen_x), 0.0f);
            if (ui::route_file_drop(session_storage->selected_rig(), timeline_scale, snap_mode, drop.path,
                                    landed, *lane, audio_imports, arrangement_error))
            {
                arrangement_error.clear();
            }
        }
        drained_drops.clear();

        // A finished decode becomes a clip here, on the thread that owns the rig.
        audio_imports.drain(finished_imports);
        for (const SushiDSP::host::AudioImport& import : finished_imports)
        {
            if (!session_storage.has_value())
            {
                continue;
            }
            if (!import.error.empty())
            {
                arrangement_error = import.error;
                continue;
            }
            if (session_storage->selected_rig().place_clip(import.track, import.wav_path,
                                                   import.start_frame, arrangement_error))
            {
                arrangement_error.clear();
            }
        }
        finished_imports.clear();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        const ImGuiWindowFlags region_flags =
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoBringToFrontOnFocus;

        int display_w = 0;
        int display_h = 0;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        const float width = static_cast<float>(display_w);
        const float height = static_cast<float>(display_h);

        // Only the bottom divider has a window of its own; see applications/host_gui/README.md.
        const ImVec2 no_spacing(0.0f, 0.0f);
        const float top_bar_height = 40.0f;
        const float divider_thickness = 14.0f;

        // A seam carries no grab handle, so it is 5 px thick; see applications/host_gui/README.md.
        const float seam_thickness = 5.0f;
        const ui::DividerLimits bottom_limits{72.0f, 200.0f};
        const ui::DividerLimits settings_limits{120.0f, 140.0f};
        const ui::DividerLimits gear_limits{220.0f, 360.0f};

        // The mini-DAW takes the whole workspace, so the workspace switch is drawn without it.
        const float switch_height = mini_daw_open ? 0.0f : 30.0f;
        const float vertical_available =
            std::max(height - top_bar_height - switch_height - seam_thickness, 0.0f);
        bottom_bar_height = ui::fit_pane_size(bottom_bar_height, vertical_available, bottom_limits);
        const float workspace_height = std::max(vertical_available - bottom_bar_height, 0.0f);

        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowSize(ImVec2(width, top_bar_height));
        ImGui::Begin("##top", nullptr, region_flags);
        {
            bool rig_replaced = false;
            ui::DrawTopBar(session_storage.has_value() ? &*session_storage : nullptr,
                           session_storage.has_value(), device_label, open_device_settings,
                           status, rig_replaced);
            if (rig_replaced && session_storage.has_value())
            {
                // A load numbers the new rig's slots from zero, so nothing may carry across.
                seed_rig_chain(session_storage->selected_rig(), rig_chain_state, status);
                room_error.clear();
                cabinet_section_state = ui::CabinetSectionState();
            }
        }
        ImGui::End();

        if (!mini_daw_open)
        {
            ImGui::SetNextWindowPos(ImVec2(0.0f, top_bar_height));
            ImGui::SetNextWindowSize(ImVec2(width, switch_height));
            ImGui::Begin("##workspace_switch", nullptr,
                         region_flags | ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoScrollWithMouse);
            if (ImGui::RadioButton("Rig", !cabinet_section_open))
            {
                cabinet_section_open = false;
            }
            ImGui::SameLine(0.0f, 16.0f);
            if (ImGui::RadioButton("Cabinet section", cabinet_section_open))
            {
                cabinet_section_open = true;
            }
            ImGui::End();
        }

        ImGui::SetNextWindowPos(ImVec2(0.0f, top_bar_height + switch_height));
        ImGui::SetNextWindowSize(ImVec2(width, workspace_height));
        if (mini_daw_open)
        {
            // The wheel belongs to the timeline's own scroll and zoom.
            ImGui::Begin("##arrangement", nullptr,
                         region_flags | ImGuiWindowFlags_NoScrollWithMouse);
            if (session_storage.has_value())
            {
                ui::DrawArrangementView(*session_storage, timeline_scale, snap_mode,
                                        audio_imports, arrangement_error, lane_geometry,
                                        arrangement_header_width,
                                        session_bounce, default_hrtf_path);
            }
            ImGui::End();
        }
        else if (cabinet_section_open)
        {
            ImGui::Begin("##cabinet_section", nullptr, region_flags);
            if (session_storage.has_value())
            {
                ui::DrawCabinetSectionPane(session_storage->selected_rig(), rig_chain_state.topology,
                                           rig_chain_state.applier, cabinet_section_state);
            }
            ImGui::End();
        }
        else
        {
            const ImGuiWindowFlags frame_flags =
                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
            const ImVec2 pane_padding = ImGui::GetStyle().WindowPadding;
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, no_spacing);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, no_spacing);
            ImGui::Begin("##workspace", nullptr, region_flags | frame_flags);

            const float horizontal_available = std::max(width - seam_thickness, 0.0f);
            gear_width = ui::fit_pane_size(gear_width, horizontal_available, gear_limits);
            const float left_width = std::max(horizontal_available - gear_width, 0.0f);
            const float column_available = std::max(workspace_height - divider_thickness, 0.0f);
            settings_height = ui::fit_pane_size(settings_height, column_available, settings_limits);
            const float chain_height = std::max(column_available - settings_height, 0.0f);

            ImGui::BeginChild("##left", ImVec2(left_width, workspace_height), false, frame_flags);

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, pane_padding);
            ImGui::BeginChild("##settings", ImVec2(0.0f, settings_height), false,
                              ImGuiWindowFlags_AlwaysUseWindowPadding);
            ImGui::PopStyleVar(3);
            if (session_storage.has_value())
            {
                const std::optional<RigController::SlotId>& selected = rig_chain_state.selected;
                if (selected.has_value() && session_storage->selected_rig().hosts_room(*selected))
                {
                    ui::DrawRoomPanel(session_storage->selected_rig(), *selected, room_error);
                }
                else
                {
                    ui::DrawStagePanel(session_storage->selected_rig(), selected, default_ir_directory);
                }
            }
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, no_spacing);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, no_spacing);
            ImGui::EndChild();

            ui::DrawDivider("##settings_divider", ui::DividerAxis::Horizontal, settings_height,
                            column_available, settings_limits, false);

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, pane_padding);
            ImGui::BeginChild("##chain", ImVec2(0.0f, chain_height), false,
                              ImGuiWindowFlags_AlwaysUseWindowPadding |
                                  ImGuiWindowFlags_HorizontalScrollbar);
            ImGui::PopStyleVar(3);
            if (session_storage.has_value())
            {
                std::optional<RigController::SlotId> created_slot;
                ui::DrawRigChainView(session_storage->selected_rig(), rig_chain_state, created_slot);
                if (created_slot.has_value())
                {
                    apply_default_cabinet(session_storage->selected_rig(), *created_slot, default_ir_path);
                }
                if (!rig_chain_state.edit_error.empty())
                {
                    // Pinned to the pane's bottom-left, so the cards do not jump when it appears.
                    const ImVec2 window_origin = ImGui::GetWindowPos();
                    ImGui::SetCursorScreenPos(
                        ImVec2(window_origin.x + ImGui::GetStyle().WindowPadding.x,
                               window_origin.y + ImGui::GetWindowHeight() -
                                   ImGui::GetTextLineHeightWithSpacing()));
                    ImGui::TextColored(ui::to_imvec4(ui::rgba(220, 72, 60)), "Edit refused: %s",
                                       rig_chain_state.edit_error.c_str());
                }
            }
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, no_spacing);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, no_spacing);
            ImGui::EndChild();
            ImGui::EndChild();

            ImGui::SameLine();
            ui::DrawPaneSeparator(ui::DividerAxis::Vertical, seam_thickness);
            ImGui::SameLine();

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, pane_padding);
            ImGui::BeginChild("##gear", ImVec2(0.0f, workspace_height), false,
                              ImGuiWindowFlags_AlwaysUseWindowPadding);
            ImGui::PopStyleVar(3);
            static ui::GearBrowserState gear_browser_state;
            ui::DrawGearBrowser(gear_browser_state);
            ImGui::EndChild();
            ImGui::End();
        }

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, no_spacing);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, no_spacing);
        // A window is never shorter than the style's minimum, which would lay the seam over the bar.
        ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(1.0f, 1.0f));
        ImGui::SetNextWindowPos(ImVec2(0.0f, top_bar_height + switch_height + workspace_height));
        ImGui::SetNextWindowSize(ImVec2(width, seam_thickness));
        ImGui::Begin("##bottom_divider", nullptr,
                     region_flags | ImGuiWindowFlags_NoScrollbar |
                         ImGuiWindowFlags_NoScrollWithMouse);
        ui::DrawPaneSeparator(ui::DividerAxis::Horizontal, seam_thickness);
        ImGui::End();
        ImGui::PopStyleVar(3);

        ImGui::SetNextWindowPos(ImVec2(0.0f, height - bottom_bar_height));
        ImGui::SetNextWindowSize(ImVec2(width, bottom_bar_height));
        ImGui::Begin("##bottom", nullptr, region_flags);
        if (session_storage.has_value())
        {
            ui::DrawTransportBar(session_storage->selected_rig(), input_probe, output_probe, mini_daw_open,
                                 transport_error);
        }
        ImGui::End();

        if (ui::DrawDeviceSettings(asio_drivers, device_choice, session_storage.has_value(),
                                   connect_error, open_device_settings))
        {
            connect_device();
        }

        ImGui::Render();
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    if (device)
    {
        device->stop();
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
