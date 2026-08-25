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
#include "SushiDSP/node_registry/NodeRegistry.hpp"

#include "Preset.hpp"
#include "RigController.hpp"

#if defined(SUSHIDSP_HAVE_ASIO)
#include "SushiDSP/device/AsioAudioDevice.hpp"
#endif

int main()
{
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
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    enum class AppState { SelectingDevice, Running };
    AppState app_state = AppState::SelectingDevice;

    const std::vector<std::string> asio_drivers =
#if defined(SUSHIDSP_HAVE_ASIO)
        SushiDSP::AsioAudioDevice::enumerate_drivers();
#else
        std::vector<std::string>{};
#endif

    int selected_device_index = 0;  // 0 = "SDL2 (no input)", 1..N = asio_drivers[index-1]
    SushiDSP::ChannelSelection selected_input = SushiDSP::ChannelSelection::Left;
    SushiDSP::ChannelSelection selected_output = SushiDSP::ChannelSelection::Stereo;
    std::string connect_error;

    // Both buffers are captured BY REFERENCE in the device callback (below), and that callback
    // keeps firing on the audio thread for as long as the device runs - i.e. for every frame
    // after the one where Connect was clicked. They MUST therefore live in this outer
    // main()-body scope, not inside the `if (ImGui::Button("Connect"))` block: a
    // same-scope-as-the-button declaration would be destroyed the instant that block exits,
    // leaving the callback's captured references dangling on the very next audio callback.
    // Both start empty and are `.assign()`-ed once, inside the Connect handler, once
    // `actual.block_size` is known - never resized again afterward, so the callback that
    // captures them by reference stays allocation-free.
    std::vector<float> mono_output;
    std::vector<float> callback_silence;

    // rig_controller_storage is declared (as storage) before device so that, by C++'s reverse-
    // declaration-order destruction rule, `device` is guaranteed to be destroyed BEFORE
    // rig_controller_storage and the RigGraphs it owns - correct by construction rather than by
    // relying on the explicit device->stop() call below. RigController's constructor needs
    // `info`, which is derived from device->format(), so it cannot be constructed until after
    // `device` exists; std::optional lets us fix the *declaration* order (and hence destruction
    // order) independently of the *construction* order. mono_output/callback_silence are
    // declared even earlier, above, so they are destroyed LAST of all four - i.e. after `device`
    // is already destroyed, which is exactly correct: nothing may reference them once they're
    // gone, and `device` (whose callback references them) is already torn down by that point.
    std::optional<RigController> rig_controller_storage;
    std::unique_ptr<SushiDSP::IAudioDevice> device;

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        if (app_state == AppState::Running)
        {
            rig_controller_storage->graph_slot().reclaim();
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (app_state == AppState::SelectingDevice)
        {
            ImGui::Begin("Connect a Device");

            ImGui::Text("Device");
            if (ImGui::RadioButton("SDL2 (no input)", selected_device_index == 0))
            {
                selected_device_index = 0;
            }
            for (std::size_t i = 0; i < asio_drivers.size(); ++i)
            {
                ImGui::PushID(static_cast<int>(i));
                if (ImGui::RadioButton(asio_drivers[i].c_str(), selected_device_index == static_cast<int>(i) + 1))
                {
                    selected_device_index = static_cast<int>(i) + 1;
                }
                ImGui::PopID();
            }

            if (selected_device_index > 0)
            {
                ImGui::Separator();
                ImGui::Text("Input");
                if (ImGui::RadioButton("L##input", selected_input == SushiDSP::ChannelSelection::Left)) selected_input = SushiDSP::ChannelSelection::Left;
                ImGui::SameLine();
                if (ImGui::RadioButton("R##input", selected_input == SushiDSP::ChannelSelection::Right)) selected_input = SushiDSP::ChannelSelection::Right;

                ImGui::Text("Output");
                if (ImGui::RadioButton("L##output", selected_output == SushiDSP::ChannelSelection::Left)) selected_output = SushiDSP::ChannelSelection::Left;
                ImGui::SameLine();
                if (ImGui::RadioButton("R##output", selected_output == SushiDSP::ChannelSelection::Right)) selected_output = SushiDSP::ChannelSelection::Right;
                ImGui::SameLine();
                if (ImGui::RadioButton("stereo##output", selected_output == SushiDSP::ChannelSelection::Stereo)) selected_output = SushiDSP::ChannelSelection::Stereo;
            }

            ImGui::Separator();
            if (ImGui::Button("Connect"))
            {
                try
                {
                    if (selected_device_index == 0)
                    {
                        SushiDSP::DeviceFormat requested{ 48000.0, 256, 1, 1 };
                        device = std::make_unique<SushiDSP::Sdl2AudioDevice>(requested);
                    }
#if defined(SUSHIDSP_HAVE_ASIO)
                    else
                    {
                        SushiDSP::DeviceFormat requested{ 48000.0, 256, 2, 2 };
                        device = std::make_unique<SushiDSP::AsioAudioDevice>(
                            asio_drivers[static_cast<std::size_t>(selected_device_index - 1)], requested);
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

                    const std::size_t input_index = SushiDSP::channel_selection_input_index(selected_input);
                    const SushiDSP::ChannelSelection output_selection = selected_output;
                    const bool is_multi_channel_input = actual.input_channels >= 2;
                    const bool is_multi_channel_output = actual.output_channels >= 2;

                    // Sized once, here, off the audio thread - the outer-scope vectors declared
                    // above (mono_output, callback_silence) are never resized again after this,
                    // so the callback that captures them by reference stays allocation-free
                    // (mirrors apps/host_asio/main.cpp's own pattern).
                    mono_output.assign(actual.block_size, 0.0f);
                    callback_silence.assign(actual.block_size, 0.0f);

                    RigController& rig_controller_ref = *rig_controller_storage;
                    device->start([&rig_controller_ref, &mono_output, &callback_silence, input_index,
                                    output_selection, is_multi_channel_input, is_multi_channel_output]
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

                        float* mono_output_ptr[1] = { mono_output.data() };
                        SushiDSP::ProcessContext context{ mono_output_ptr, selected_channel, frames, 0.0 };
                        if (active != nullptr)
                        {
                            active->process(context);
                        }
                        else
                        {
                            std::fill(mono_output.begin(), mono_output.begin() + frames, 0.0f);
                        }

                        if (is_multi_channel_output)
                        {
                            SushiDSP::fan_out_mono_to_stereo(mono_output.data(), outputs, frames, output_selection);
                        }
                        else
                        {
                            std::memcpy(outputs[0], mono_output.data(), frames * sizeof(float));
                        }
                    });

                    app_state = AppState::Running;
                }
                catch (const std::exception& error)
                {
                    connect_error = error.what();
                }
            }
            if (!connect_error.empty())
            {
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", connect_error.c_str());
            }

            ImGui::End();
        }
        else
        {
        RigController& rig_controller = *rig_controller_storage;

        ImGui::Begin("SushiDSP Rig");

        ImGui::Text("Connected: %s (input %s, output %s)",
            selected_device_index == 0 ? "SDL2" : asio_drivers[static_cast<std::size_t>(selected_device_index - 1)].c_str(),
            SushiDSP::channel_selection_label(selected_input),
            SushiDSP::channel_selection_label(selected_output));
        ImGui::Separator();

        static char preset_path[256] = "preset.json";
        ImGui::InputText("Preset path", preset_path, sizeof(preset_path));
        ImGui::SameLine();
        if (ImGui::Button("Save"))
        {
            try { save_preset(rig_controller, preset_path); }
            catch (const std::exception& e) { std::fprintf(stderr, "Save failed: %s\n", e.what()); }
        }
        ImGui::SameLine();
        if (ImGui::Button("Load"))
        {
            try { load_preset(rig_controller, preset_path); }
            catch (const std::exception& e) { std::fprintf(stderr, "Load failed: %s\n", e.what()); }
        }
        ImGui::Separator();

        ImGui::Text("Rack");
        ImGui::Separator();
        {
            const auto ids = rig_controller.chain_ids();
            static int selected_slot = -1;
            for (std::size_t i = 0; i < ids.size(); ++i)
            {
                ImGui::PushID(static_cast<int>(i));
                if (ImGui::Selectable(ids[i].c_str(), selected_slot == static_cast<int>(i), 0, ImVec2(150, 0)))
                {
                    selected_slot = static_cast<int>(i);
                }
                if (ImGui::BeginDragDropSource())
                {
                    ImGui::SetDragDropPayload("RACK_SLOT_INDEX", &i, sizeof(std::size_t));
                    ImGui::Text("%s", ids[i].c_str());
                    ImGui::EndDragDropSource();
                }
                if (ImGui::BeginDragDropTarget())
                {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("RACK_SLOT_INDEX"))
                    {
                        const std::size_t from = *static_cast<const std::size_t*>(payload->Data);
                        rig_controller.move(from, i);
                        // Remap selected_slot the same way RigController::move() remaps a slot
                        // (erase at `from`, then insert at `i`): the moved element itself lands
                        // at `i`; everything strictly between the old and new position shifts by
                        // one to close/open the gap; anything else is unaffected.
                        if (selected_slot == static_cast<int>(from))
                        {
                            selected_slot = static_cast<int>(i);
                        }
                        else if (static_cast<int>(from) < selected_slot && selected_slot <= static_cast<int>(i))
                        {
                            --selected_slot;
                        }
                        else if (static_cast<int>(i) <= selected_slot && selected_slot < static_cast<int>(from))
                        {
                            ++selected_slot;
                        }
                    }
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("GEAR_DESCRIPTOR_ID"))
                    {
                        const char* descriptor_id = static_cast<const char*>(payload->Data);
                        rig_controller.insert(i, std::string(descriptor_id));
                    }
                    ImGui::EndDragDropTarget();
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("x"))
                {
                    rig_controller.remove(i);
                    if (selected_slot == static_cast<int>(i))
                    {
                        selected_slot = -1;
                    }
                    else if (selected_slot > static_cast<int>(i))
                    {
                        --selected_slot;
                    }
                }
                ImGui::PopID();
            }

            // A drop target at the end of the list, for appending.
            ImGui::Dummy(ImVec2(150, 20));
            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("GEAR_DESCRIPTOR_ID"))
                {
                    const char* descriptor_id = static_cast<const char*>(payload->Data);
                    rig_controller.append(std::string(descriptor_id));
                }
                ImGui::EndDragDropTarget();
            }

            ImGui::Separator();
            ImGui::Text("Parameters");
            if (selected_slot >= 0 && selected_slot < static_cast<int>(ids.size()))
            {
                SushiDSP::RigGraph* active = rig_controller.active_graph();
                if (active != nullptr)
                {
                    SushiDSP::IAudioNode* node = active->node(static_cast<SushiDSP::NodeHandle>(selected_slot));
                    for (const auto& entry : SushiDSP::node_registry())
                    {
                        if (ids[static_cast<std::size_t>(selected_slot)] == entry.descriptor->id)
                        {
                            for (std::size_t p = 0; p < entry.descriptor->parameter_count; ++p)
                            {
                                const SushiDSP::ParameterDescriptor& param = entry.descriptor->parameters[p];
                                float value = node != nullptr ? node->get_parameter(param.id) : param.default_value;
                                if (ImGui::SliderFloat(param.label, &value, param.min, param.max))
                                {
                                    rig_controller.set_parameter(static_cast<std::size_t>(selected_slot), param.id, value);
                                }
                            }
                            break;
                        }
                    }
                }
            }
        }

        ImGui::Separator();
        ImGui::Text("Gear");
        {
            const char* current_category = nullptr;
            for (const auto& entry : SushiDSP::node_registry())
            {
                if (current_category == nullptr || std::strcmp(current_category, entry.category.name) != 0)
                {
                    current_category = entry.category.name;
                    ImGui::Text("%s", current_category);
                }
                ImGui::Button(entry.descriptor->name);
                if (ImGui::BeginDragDropSource())
                {
                    ImGui::SetDragDropPayload("GEAR_DESCRIPTOR_ID", entry.descriptor->id, std::strlen(entry.descriptor->id) + 1);
                    ImGui::Text("%s", entry.descriptor->name);
                    ImGui::EndDragDropSource();
                }
            }
        }

        ImGui::End();
        }

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
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
