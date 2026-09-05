/**************************************************************************/
/* main.cpp                                                               */
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

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include "SushiDSP/core/IAudioDevice.hpp"
#include "SushiDSP/core/RigGraph.hpp"
#include "SushiDSP/core/RtGraphSlot.hpp"
#include "SushiDSP/device/ChannelSelection.hpp"
#include "SushiDSP/device/Sdl2AudioDevice.hpp"

#include "RigController.hpp"
#include "ui/ChainStrip.hpp"
#include "ui/DeviceSettings.hpp"
#include "ui/GearBrowser.hpp"
#include "ui/LevelProbe.hpp"
#include "ui/MeterBar.hpp"
#include "ui/StagePanel.hpp"
#include "ui/Theme.hpp"
#include "ui/TopBar.hpp"

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

    /// @brief Installs the bundled cabinet impulse response on a freshly-created JCM800 slot,
    /// if the bundle is present next to the executable. Warns once, on stderr, when it is not.
    /// @param controller The rig to act on.
    /// @param slot The new slot's id.
    /// @param default_ir_path Path to the bundled impulse response.
    void apply_default_cabinet(RigController& controller, RigController::SlotId slot, const std::string& default_ir_path)
    {
        if (controller.descriptor_id(slot) != "sushidsp.jcm800")
        {
            return;
        }

        static bool warned = false;
        std::ifstream probe(default_ir_path, std::ios::binary);
        if (!probe)
        {
            if (!warned)
            {
                warned = true;
                std::fprintf(stderr,
                    "No bundled cabinet impulse response at '%s' - JCM800 slots start without a "
                    "cabinet. Drop a 4x12 IR there, or load one from the Cabinet IR field.\n",
                    default_ir_path.c_str());
            }
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
}

int main(int argc, char** argv)
{
    const std::string default_ir_path =
        executable_directory(argc > 0 ? argv[0] : nullptr) + "assets/ir/default_4x12.wav";

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
    std::optional<RigController::SlotId> selected_slot;
    std::string device_label = "not connected";
    std::string status;
    std::string connect_error;
    bool open_device_settings = true;

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
    std::vector<float> mono_output;
    std::vector<float> callback_silence;
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
                           &rig_controller_storage, &mono_output, &callback_silence, &input_probe,
                           &output_probe, &device_label, &selected_slot, &open_device_settings]()
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
                SushiDSP::DeviceFormat requested{ 48000.0, 256, 1, 1 };
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
            // The rig itself stays mono (1 in, 1 out) - channel selection only decides
            // which physical ASIO channel feeds it and which physical channel(s) its
            // output reaches (mirrors apps/host_asio/main.cpp's own pattern). The rig
            // always renders into the one-element mono_output_ptr array below, so this
            // must be 1 regardless of actual.output_channels.
            SushiDSP::PrepareInfo info{ actual.sample_rate, actual.block_size, 1 };
            rig_controller_storage.emplace(info);

            const std::size_t input_index = SushiDSP::channel_selection_input_index(device_choice.input);
            const SushiDSP::ChannelSelection output_selection = device_choice.output;
            const bool is_multi_channel_input = actual.input_channels >= 2;
            const bool is_multi_channel_output = actual.output_channels >= 2;

            // Sized once, here, off the audio thread - the outer-scope vectors declared
            // above (mono_output, callback_silence) are never resized again after this,
            // so the callback that captures them by reference stays allocation-free
            // (mirrors apps/host_asio/main.cpp's own pattern).
            mono_output.assign(actual.block_size, 0.0f);
            callback_silence.assign(actual.block_size, 0.0f);

            RigController& rig_controller_ref = *rig_controller_storage;
            device->start([&rig_controller_ref, &mono_output, &callback_silence, &input_probe,
                            &output_probe, input_index, output_selection, is_multi_channel_input,
                            is_multi_channel_output]
                (const float* const* inputs, float* const* outputs, std::uint32_t frames)
            {
                if (frames > mono_output.size())
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

                float* mono_output_ptr[1] = { mono_output.data() };
                SushiDSP::ProcessContext context{ mono_output_ptr, selected_channel, frames, 0.0 };
                if (active == nullptr)
                {
                    std::fill(mono_output.begin(), mono_output.begin() + frames, 0.0f);
                }
                else if (active->is_empty())
                {
                    // Every slot is bypassed, so the rig is a wire.
                    std::memcpy(mono_output.data(), selected_channel[0], frames * sizeof(float));
                }
                else
                {
                    active->process(context);
                }

                output_probe.report(mono_output.data(), frames);

                if (is_multi_channel_output)
                {
                    SushiDSP::fan_out_mono_to_stereo(mono_output.data(), outputs, frames, output_selection);
                }
                else
                {
                    std::memcpy(outputs[0], mono_output.data(), frames * sizeof(float));
                }
            });

            device_label = std::string("Connected: ")
                + (device_choice.device_index == 0
                    ? "SDL2"
                    : asio_drivers[static_cast<std::size_t>(device_choice.device_index - 1)])
                + " (input " + SushiDSP::channel_selection_label(device_choice.input)
                + ", output " + SushiDSP::channel_selection_label(device_choice.output) + ")";
            selected_slot = std::nullopt;
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
        const float top_height = 48.0f;
        const float meter_height = 40.0f;
        const float browser_width = 300.0f;
        const float body_height = height - top_height - meter_height;
        const float stage_height = body_height * 0.58f;

        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowSize(ImVec2(width, top_height));
        ImGui::Begin("##top", nullptr, kRegionFlags);
        if (rig_controller_storage.has_value())
        {
            ui::DrawTopBar(*rig_controller_storage, device_label, open_device_settings, status);
        }
        else
        {
            ImGui::TextColored(ui::to_imvec4(ui::palette().muted_text), "No device connected.");
            ImGui::SameLine(width - 96.0f);
            if (ImGui::Button("Settings"))
            {
                open_device_settings = true;
            }
        }
        ImGui::End();

        ImGui::SetNextWindowPos(ImVec2(0.0f, top_height));
        ImGui::SetNextWindowSize(ImVec2(width - browser_width, stage_height));
        ImGui::Begin("##stage", nullptr, kRegionFlags);
        if (rig_controller_storage.has_value())
        {
            ui::DrawStagePanel(*rig_controller_storage, selected_slot);
        }
        ImGui::End();

        ImGui::SetNextWindowPos(ImVec2(0.0f, top_height + stage_height));
        ImGui::SetNextWindowSize(ImVec2(width - browser_width, body_height - stage_height));
        ImGui::Begin("##chain", nullptr, kRegionFlags | ImGuiWindowFlags_HorizontalScrollbar);
        if (rig_controller_storage.has_value())
        {
            std::optional<RigController::SlotId> created_slot;
            ui::DrawChainStrip(*rig_controller_storage, selected_slot, created_slot);
            if (created_slot.has_value())
            {
                apply_default_cabinet(*rig_controller_storage, *created_slot, default_ir_path);
            }
        }
        ImGui::End();

        ImGui::SetNextWindowPos(ImVec2(width - browser_width, top_height));
        ImGui::SetNextWindowSize(ImVec2(browser_width, body_height));
        ImGui::Begin("##gear", nullptr, kRegionFlags);
        ui::DrawGearBrowser();
        ImGui::End();

        ImGui::SetNextWindowPos(ImVec2(0.0f, height - meter_height));
        ImGui::SetNextWindowSize(ImVec2(width, meter_height));
        ImGui::Begin("##meters", nullptr, kRegionFlags);
        ui::DrawMeterBar(input_probe, output_probe);
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
