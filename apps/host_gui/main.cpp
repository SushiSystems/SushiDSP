/****************************************************************/
/* main.cpp                                                     */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

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

#include "SushiDSP/core/IAudioDevice.hpp"
#include "SushiDSP/core/RigGraph.hpp"
#include "SushiDSP/core/RtGraphSlot.hpp"
#include "SushiDSP/audio_format/ChannelSelection.hpp"
#include "SushiDSP/device/Sdl2AudioDevice.hpp"

#include "AudioImportCache.hpp"
#include "AudioImportQueue.hpp"
#include "FileDropQueue.hpp"
#include "RigController.hpp"
#include "RigTopology.hpp"
#include "TopologyApplier.hpp"
#include "ui/ArrangementLayout.hpp"
#include "ui/ArrangementView.hpp"
#include "ui/DeviceSettings.hpp"
#include "ui/Divider.hpp"
#include "ui/DividerMath.hpp"
#include "ui/GearBrowser.hpp"
#include "ui/LevelProbe.hpp"
#include "ui/MeterBar.hpp"
#include "ui/RigChainView.hpp"
#include "ui/RoomPanel.hpp"
#include "ui/StagePanel.hpp"
#include "ui/Theme.hpp"
#include "ui/TopBar.hpp"
#include "ui/TransportBar.hpp"

#if defined(SUSHIDSP_HAVE_ASIO)
#include "SushiDSP/device/AsioAudioDevice.hpp"
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
    /// @see docs/archive/superpowers/specs/2026-09-08-arrangement-view-design.md §7
    void queue_dropped_files(GLFWwindow* window, int count, const char** paths)
    {
        auto* queue =
            static_cast<SushiDSP::host::FileDropQueue*>(glfwGetWindowUserPointer(window));
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
            cursor_y *= static_cast<double>(framebuffer_height) / static_cast<double>(window_height);
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
    void apply_default_hrtf(RigController& controller, const std::string& default_hrtf_path)
    {
        std::string error;
        if (controller.set_hrtf_path(default_hrtf_path, error))
        {
            return;
        }
        std::fprintf(stderr,
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
    void apply_default_cabinet(RigController& controller, RigController::SlotId slot, const std::string& default_ir_path)
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
            std::fprintf(stderr, "Bundled cabinet impulse response failed to load: %s\n", error.what());
        }
    }

    /// @brief Reports whether the rig's edges are exactly the linear run of its slot order.
    /// @param rig The rig to inspect.
    /// @return True when every consecutive pair of slot_order() is an edge and no other edge exists.
    bool rig_is_linear(const RigController& rig)
    {
        const std::vector<RigController::SlotId> order = rig.slot_order();
        const std::vector<std::pair<RigController::SlotId, RigController::SlotId>> edges = rig.edges();
        const std::size_t expected = order.empty() ? 0 : order.size() - 1;
        if (edges.size() != expected)
        {
            return false;
        }
        for (std::size_t index = 0; index < expected; ++index)
        {
            const std::pair<RigController::SlotId, RigController::SlotId> link(order[index], order[index + 1]);
            if (std::find(edges.begin(), edges.end(), link) == edges.end())
            {
                return false;
            }
        }
        return true;
    }

    /// @brief Resets the chain view's state to a Series chain of the rig's slot order.
    /// @param rig The rig the state now describes; flattened to series when its graph is not linear.
    /// @param state The chain view state to reset.
    /// @param status Out: a message when the rig's shape was flattened or flattening failed.
    void seed_rig_chain(RigController& rig, ui::RigChainState& state, std::string& status)
    {
        state.topology = RigTopology::from_linear_order(rig.slot_order());
        state.applier = TopologyApplier();
        state.selected = std::nullopt;
        state.edit_error.clear();
        if (rig_is_linear(rig))
        {
            return;
        }
        try
        {
            state.applier.apply(state.topology, rig);
            status = "Preset had a parallel shape; loaded as a series chain.";
        }
        catch (const std::exception& error)
        {
            status = error.what();
        }
    }
}

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

    ui::DeviceChoice device_choice{ 0, SushiDSP::ChannelSelection::Left,
                                    SushiDSP::ChannelSelection::Stereo };
    ui::RigChainState rig_chain_state;
    std::string device_label = "not connected";
    std::string status;
    std::string connect_error;
    std::string transport_error;
    std::string arrangement_error;
    std::string room_error;
    bool open_device_settings = true;
    // The mini-DAW (arrangement view + meter bar) starts hidden.
    bool mini_daw_open = false;

    // The zoom, the scroll and the snap belong to the GUI loop rather than to the rig: they are
    // how the arrangement is being looked at, not what it plays.
    ui::TimelineScale timeline_scale;
    ui::SnapMode      snap_mode = ui::SnapMode::Bar;
    ui::LaneGeometry  lane_geometry;
    float             arrangement_header_width = 196.0f;

    // Pane sizes the dividers drag; each is re-fitted to the window every frame.
    float bottom_bar_height = 96.0f;
    float settings_height = 430.0f;
    float gear_width = 300.0f;

    // Sized once by the first Explorer drop and reused, so a drop costs no allocation after it.
    std::vector<SushiDSP::host::FileDrop> drained_drops;

    // A dropped mp3 is decoded to a WAV in a folder of the host's own, beside the impulse
    // responses it already keeps next to the executable, and the clip points at that WAV: a
    // session stores a clip as a path, so the decode has to leave a file behind, and it must not
    // be written into the user's music folder. The decode runs on the queue's worker thread, so
    // a five-minute song does not stop the window.
    SushiDSP::host::AudioImportQueue audio_imports{ SushiDSP::host::AudioImportCache(
        executable_directory(argc > 0 ? argv[0] : nullptr) + "cache/imported_audio") };
    std::vector<SushiDSP::host::AudioImport> finished_imports;

    // Both buffers are captured BY REFERENCE in the device callback (below), and that callback
    // keeps firing on the audio thread for as long as the device runs - i.e. for every frame
    // after the one where Connect was clicked. They MUST therefore live in this outer
    // main()-body scope, not inside the connect_device lambda: a lambda-body declaration would
    // be destroyed the instant the connect sequence returns, leaving the callback's captured
    // references dangling on the very next audio callback. Both start empty and are
    // `.assign()`-ed once, inside connect_device, once `actual.block_size` is known - never
    // resized again afterward, so the callback that captures them by reference stays
    // allocation-free. The two level probes sit here for the same lifetime reason: the callback
    // reports a peak into each of them on every block.
    std::vector<float>        rig_input;
    std::vector<float>        rig_output;
    std::vector<float*>       rig_output_pointers;
    std::vector<const float*> rig_output_const_pointers;
    std::vector<float>        callback_silence;
    ui::LevelProbe input_probe;
    ui::LevelProbe output_probe;

    // rig_controller_storage is declared (as storage) before device so that, by C++'s reverse-
    // declaration-order destruction rule, `device` is guaranteed to be destroyed BEFORE
    // rig_controller_storage and the RigGraphs it owns - correct by construction rather than by
    // relying on the explicit device->stop() call below. RigController's constructor needs
    // `info`, which is derived from device->format(), so it cannot be constructed until after
    // `device` exists; std::optional lets us fix the *declaration* order (and hence destruction
    // order) independently of the *construction* order. mono_output/callback_silence and the two
    // probes are declared even earlier, above, so they are destroyed LAST of all - i.e. after
    // `device` is already destroyed, which is exactly correct: nothing may reference them once
    // they're gone, and `device` (whose callback references them) is already torn down by that
    // point.
    std::optional<RigController> rig_controller_storage;
    std::unique_ptr<SushiDSP::IAudioDevice> device;

    /// @brief Opens the chosen device, builds a rig for its format and starts the audio callback.
    /// Records the failure in connect_error and leaves the previous rig gone on any exception.
    auto connect_device = [&device, &device_choice, &asio_drivers, &connect_error,
                           &rig_controller_storage, &rig_input, &rig_output, &rig_output_pointers,
                           &rig_output_const_pointers, &callback_silence, &input_probe,
                           &output_probe, &device_label, &rig_chain_state, &status,
                           &open_device_settings, &default_hrtf_path]()
    {
        connect_error.clear();

        // Destroy any previous (possibly partially-set-up) device FIRST, before
        // constructing a new one. AsioAudioDevice/Sdl2AudioDevice both wrap
        // process-wide SDK state; std::unique_ptr::operator= would otherwise
        // evaluate the new construction (ASIOInit/ASIOCreateBuffers etc.) while
        // the old device is still alive, then tear that old state down only
        // afterwards - destroying the SDK state the new device just built.
        device.reset();

        try
        {
            if (device_choice.device_index == 0)
            {
                SushiDSP::DeviceFormat requested{ 48000.0, 256, 1, 2 };
                device = std::make_unique<SushiDSP::Sdl2AudioDevice>(requested);
            }
#if defined(SUSHIDSP_HAVE_ASIO)
            else
            {
                SushiDSP::DeviceFormat requested{ 48000.0, 256, 2, 2 };
                device = std::make_unique<SushiDSP::AsioAudioDevice>(
                    asio_drivers[static_cast<std::size_t>(device_choice.device_index - 1)], requested);
            }
#endif

            const SushiDSP::DeviceFormat actual = device->format();
            // The rig's input is always one channel - an amplifier is a mono circuit. Its output
            // is as wide as the device can take, up to RigGraph::kMaxChannels, and RigGraph
            // adapts a mono terminal node to that width itself. Channel selection still decides
            // which physical channel feeds the rig and which receives its output.
            const std::uint32_t rig_channels =
                std::min<std::uint32_t>(std::max(1u, actual.output_channels),
                                        SushiDSP::RigGraph::kMaxChannels);
            SushiDSP::PrepareInfo info{ actual.sample_rate, actual.block_size, rig_channels };
            rig_controller_storage.emplace(info);
            apply_default_hrtf(*rig_controller_storage, default_hrtf_path);
            seed_rig_chain(*rig_controller_storage, rig_chain_state, status);

            const std::size_t input_index = SushiDSP::channel_selection_input_index(device_choice.input);
            const SushiDSP::ChannelSelection output_selection = device_choice.output;
            const bool is_multi_channel_input = actual.input_channels >= 2;
            const std::uint32_t device_output_channels = std::max(1u, actual.output_channels);
            if (!SushiDSP::rig_output_adapts_to_device(rig_channels, device_output_channels))
            {
                throw std::runtime_error(
                    "no channel adaptation rule from the rig's " + std::to_string(rig_channels)
                    + " output channels to the device's " + std::to_string(device_output_channels));
            }

            // Sized once, here, off the audio thread - the outer-scope vectors declared
            // above (mono_output, callback_silence) are never resized again after this,
            // so the callback that captures them by reference stays allocation-free
            // (mirrors apps/host_asio/main.cpp's own pattern).
            rig_input.assign(actual.block_size, 0.0f);
            rig_output.assign(static_cast<std::size_t>(actual.block_size) * rig_channels, 0.0f);
            rig_output_pointers.resize(rig_channels);
            rig_output_const_pointers.resize(rig_channels);
            for (std::uint32_t channel = 0; channel < rig_channels; ++channel)
            {
                float* start = rig_output.data() + static_cast<std::size_t>(channel) * actual.block_size;
                rig_output_pointers[channel] = start;
                rig_output_const_pointers[channel] = start;
            }
            callback_silence.assign(actual.block_size, 0.0f);

            RigController& rig_controller_ref = *rig_controller_storage;
            device->start([&rig_controller_ref, &rig_input, &rig_output, &rig_output_pointers,
                            &rig_output_const_pointers, &callback_silence, &input_probe,
                            &output_probe, input_index, output_selection, is_multi_channel_input,
                            device_output_channels, rig_channels]
                (const float* const* inputs, float* const* outputs, std::uint32_t frames)
            {
                if (frames > rig_input.size())
                {
                    return;
                }
                SushiDSP::RigGraph* active = rig_controller_ref.graph_slot().acquire_for_audio_thread();

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

                SushiDSP::Transport& transport = rig_controller_ref.transport();
                const bool rolling = transport.state() != SushiDSP::TransportState::Stopped;
                const RigInputSource source = rig_controller_ref.input_source();

                float* rig_input_pointer[1] = { rig_input.data() };
                if (source == RigInputSource::Track)
                {
                    std::memset(rig_input.data(), 0, frames * sizeof(float));
                }
                else
                {
                    std::memcpy(rig_input.data(), selected_channel[0], frames * sizeof(float));
                }
                if (source != RigInputSource::LiveInput && rolling)
                {
                    rig_controller_ref.reamp_player().process(rig_input_pointer, 1, frames, transport);
                }

                const float* rig_input_const_pointer[1] = { rig_input.data() };
                SushiDSP::ProcessContext context{ rig_output_pointers.data(),
                                                  rig_input_const_pointer, frames, 0.0 };
                if (active == nullptr)
                {
                    for (std::uint32_t channel = 0; channel < rig_channels; ++channel)
                    {
                        std::memset(rig_output_pointers[channel], 0, frames * sizeof(float));
                    }
                }
                else if (active->is_empty())
                {
                    // A rack holding nothing, or one whose every slot is bypassed: both are a
                    // cable, so the input reaches every output channel untouched.
                    for (std::uint32_t channel = 0; channel < rig_channels; ++channel)
                    {
                        std::memcpy(rig_output_pointers[channel], rig_input.data(),
                                    frames * sizeof(float));
                    }
                }
                else
                {
                    active->process(context);
                }

                if (rolling)
                {
                    rig_controller_ref.player().process(rig_output_pointers.data(), rig_channels,
                                                       frames, transport);
                }

                output_probe.report(rig_output_pointers[0], frames);

                SushiDSP::RecordingBus& bus = rig_controller_ref.recording_bus();
                if (bus.is_recording())
                {
                    // The dry leg is the buffer that actually drove the rig, re-amp included.
                    bus.write(rig_output_const_pointers.data(), rig_input.data(), frames);
                }

                // Added after both recorder legs are written, so the click reaches no recorded file.
                // @see docs/archive/superpowers/specs/2026-09-08-arrangement-view-design.md §4
                rig_controller_ref.metronome().process(rig_output_pointers.data(), rig_channels,
                                                       frames, transport,
                                                       rig_controller_ref.musical_clock());

                if (rolling)
                {
                    transport.advance(frames);
                }

                SushiDSP::write_rig_output_to_device(rig_output_const_pointers.data(), rig_channels,
                                                     outputs, device_output_channels, frames,
                                                     output_selection);
            });

            device_label = std::string("Connected: ")
                + (device_choice.device_index == 0
                    ? "SDL2"
                    : asio_drivers[static_cast<std::size_t>(device_choice.device_index - 1)])
                + " (input " + SushiDSP::channel_selection_label(device_choice.input)
                + ", output " + SushiDSP::channel_selection_label(device_choice.output) + ")";
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
        if (rig_controller_storage.has_value())
        {
            rig_controller_storage->reclaim();
        }

        // Explorer's drops are resolved against the lane rectangles the previous frame recorded,
        // then routed through the function an ImGui payload drop also calls.
        file_drops.drain(drained_drops);
        for (const SushiDSP::host::FileDrop& drop : drained_drops)
        {
            if (!rig_controller_storage.has_value())
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
            if (ui::route_file_drop(*rig_controller_storage, timeline_scale, snap_mode, drop.path,
                                    landed, *lane, audio_imports, arrangement_error))
            {
                arrangement_error.clear();
            }
        }
        drained_drops.clear();

        // A decode that finished on the worker thread becomes a clip here, on the thread that
        // owns the rig.
        audio_imports.drain(finished_imports);
        for (const SushiDSP::host::AudioImport& import : finished_imports)
        {
            if (!rig_controller_storage.has_value())
            {
                continue;
            }
            if (!import.error.empty())
            {
                arrangement_error = import.error;
                continue;
            }
            if (rig_controller_storage->place_clip(import.track, import.wav_path,
                                                   import.start_frame, arrangement_error))
            {
                arrangement_error.clear();
            }
        }
        finished_imports.clear();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        const ImGuiWindowFlags kRegionFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse
            | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;

        int display_w = 0;
        int display_h = 0;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        const float width = static_cast<float>(display_w);
        const float height = static_cast<float>(display_h);

        // The bottom divider lives in a thin window of its own between the workspace and the
        // bottom bar; the workspace's two dividers live inside it, between its child panes.
        const ImVec2 kNoSpacing(0.0f, 0.0f);
        const float top_bar_height = 40.0f;
        const float divider_thickness = 14.0f;

        // A seam between panes that do not resize carries no grab handle, so it needs only
        // enough room for its hairline; at the grab strip's 14 px it read as a black band.
        const float seam_thickness = 5.0f;
        const ui::DividerLimits bottom_limits{ 72.0f, 200.0f };
        const ui::DividerLimits settings_limits{ 120.0f, 140.0f };
        const ui::DividerLimits gear_limits{ 220.0f, 360.0f };

        const float vertical_available = std::max(height - top_bar_height - seam_thickness, 0.0f);
        bottom_bar_height = ui::fit_pane_size(bottom_bar_height, vertical_available, bottom_limits);
        const float workspace_height = std::max(vertical_available - bottom_bar_height, 0.0f);

        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowSize(ImVec2(width, top_bar_height));
        ImGui::Begin("##top", nullptr, kRegionFlags);
        {
            bool rig_replaced = false;
            ui::DrawTopBar(rig_controller_storage.has_value() ? &*rig_controller_storage
                                                               : nullptr,
                           rig_controller_storage.has_value(), device_label,
                           open_device_settings, status, rig_replaced);
            if (rig_replaced && rig_controller_storage.has_value())
            {
                // A load numbers the new rig's slots from zero, so nothing may carry across.
                seed_rig_chain(*rig_controller_storage, rig_chain_state, status);
                room_error.clear();
            }
        }
        ImGui::End();

        ImGui::SetNextWindowPos(ImVec2(0.0f, top_bar_height));
        ImGui::SetNextWindowSize(ImVec2(width, workspace_height));
        if (mini_daw_open)
        {
            // The wheel belongs to the timeline's own scroll and zoom.
            ImGui::Begin("##arrangement", nullptr,
                         kRegionFlags | ImGuiWindowFlags_NoScrollWithMouse);
            if (rig_controller_storage.has_value())
            {
                ui::DrawArrangementView(*rig_controller_storage, timeline_scale, snap_mode,
                                        audio_imports, arrangement_error, lane_geometry,
                                        arrangement_header_width);
            }
            ImGui::End();
        }
        else
        {
            const ImGuiWindowFlags kFrameFlags =
                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
            const ImVec2 pane_padding = ImGui::GetStyle().WindowPadding;
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, kNoSpacing);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, kNoSpacing);
            ImGui::Begin("##workspace", nullptr, kRegionFlags | kFrameFlags);

            const float horizontal_available = std::max(width - seam_thickness, 0.0f);
            gear_width = ui::fit_pane_size(gear_width, horizontal_available, gear_limits);
            const float left_width = std::max(horizontal_available - gear_width, 0.0f);
            const float column_available = std::max(workspace_height - divider_thickness, 0.0f);
            settings_height = ui::fit_pane_size(settings_height, column_available, settings_limits);
            const float chain_height = std::max(column_available - settings_height, 0.0f);

            ImGui::BeginChild("##left", ImVec2(left_width, workspace_height), false, kFrameFlags);

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, pane_padding);
            ImGui::BeginChild("##settings", ImVec2(0.0f, settings_height), false,
                              ImGuiWindowFlags_AlwaysUseWindowPadding);
            ImGui::PopStyleVar(3);
            if (rig_controller_storage.has_value())
            {
                const std::optional<RigController::SlotId>& selected = rig_chain_state.selected;
                if (selected.has_value() && rig_controller_storage->hosts_room(*selected))
                {
                    ui::DrawRoomPanel(*rig_controller_storage, *selected, room_error);
                }
                else
                {
                    ui::DrawStagePanel(*rig_controller_storage, selected, default_ir_directory);
                }
            }
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, kNoSpacing);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, kNoSpacing);
            ImGui::EndChild();

            ui::DrawDivider("##settings_divider", ui::DividerAxis::Horizontal, settings_height,
                            column_available, settings_limits, false);

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, pane_padding);
            ImGui::BeginChild("##chain", ImVec2(0.0f, chain_height), false,
                              ImGuiWindowFlags_AlwaysUseWindowPadding
                                  | ImGuiWindowFlags_HorizontalScrollbar);
            ImGui::PopStyleVar(3);
            if (rig_controller_storage.has_value())
            {
                std::optional<RigController::SlotId> created_slot;
                ui::DrawRigChainView(*rig_controller_storage, rig_chain_state, created_slot);
                if (created_slot.has_value())
                {
                    apply_default_cabinet(*rig_controller_storage, *created_slot, default_ir_path);
                }
                if (!rig_chain_state.edit_error.empty())
                {
                    // Pinned to the pane's bottom-left, so the cards do not jump when it appears.
                    const ImVec2 window_origin = ImGui::GetWindowPos();
                    ImGui::SetCursorScreenPos(
                        ImVec2(window_origin.x + ImGui::GetStyle().WindowPadding.x,
                               window_origin.y + ImGui::GetWindowHeight()
                                   - ImGui::GetTextLineHeightWithSpacing()));
                    ImGui::TextColored(ui::to_imvec4(ui::rgba(220, 72, 60)), "Edit refused: %s",
                                       rig_chain_state.edit_error.c_str());
                }
            }
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, kNoSpacing);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, kNoSpacing);
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

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, kNoSpacing);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, kNoSpacing);
        ImGui::SetNextWindowPos(ImVec2(0.0f, top_bar_height + workspace_height));
        ImGui::SetNextWindowSize(ImVec2(width, seam_thickness));
        ImGui::Begin("##bottom_divider", nullptr,
                     kRegionFlags | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ui::DrawPaneSeparator(ui::DividerAxis::Horizontal, seam_thickness);
        ImGui::End();
        ImGui::PopStyleVar(2);

        ImGui::SetNextWindowPos(ImVec2(0.0f, height - bottom_bar_height));
        ImGui::SetNextWindowSize(ImVec2(width, bottom_bar_height));
        ImGui::Begin("##bottom", nullptr, kRegionFlags);
        if (rig_controller_storage.has_value())
        {
            ui::DrawTransportBar(*rig_controller_storage, input_probe, output_probe,
                                 mini_daw_open, transport_error);
        }
        ImGui::End();

        if (ui::DrawDeviceSettings(asio_drivers, device_choice, rig_controller_storage.has_value(),
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
