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

#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include "SushiDSP/core/IAudioDevice.hpp"
#include "SushiDSP/core/RigGraph.hpp"
#include "SushiDSP/core/RtGraphSlot.hpp"
#include "SushiDSP/device/Sdl2AudioDevice.hpp"
#include "SushiDSP/node_registry/NodeRegistry.hpp"

#include "RigController.hpp"

#if SUSHIDSP_HAVE_ASIO
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

    SushiDSP::DeviceFormat requested{ 48000.0, 256, 1, 1 };
    std::unique_ptr<SushiDSP::IAudioDevice> device = std::make_unique<SushiDSP::Sdl2AudioDevice>(requested);
    const SushiDSP::DeviceFormat actual = device->format();

    SushiDSP::PrepareInfo info{ actual.sample_rate, actual.block_size, actual.output_channels };
    RigController rig_controller(info);

    // Sdl2AudioDevice is playback-only (format().input_channels == 0) and always hands the
    // callback a nullptr inputs array - a valid IAudioDevice contract (see IAudioDevice.hpp).
    // TubeScreamer, like any effect node, unconditionally reads its input channel, so the root
    // node of a rig with no live capture needs a real (silent) buffer rather than nullptr. This
    // silence buffer is sized once up front (non-RT) and only ever read on the audio thread, so
    // feeding it in place of a missing capture device stays allocation-free and RT-safe.
    std::vector<float> silence(actual.block_size, 0.0f);

    device->start([&rig_controller, &silence](const float* const* inputs, float* const* outputs, std::uint32_t frames)
    {
        SushiDSP::RigGraph* active = rig_controller.graph_slot().acquire_for_audio_thread();
        const float* silence_channel = silence.data();
        const float* const* effective_inputs = (inputs != nullptr) ? inputs : &silence_channel;
        SushiDSP::ProcessContext context{ outputs, effective_inputs, frames, 0.0 };
        if (active != nullptr)
        {
            active->process(context);
        }
    });

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        rig_controller.graph_slot().reclaim();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("SushiDSP Rig");

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

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    device->stop();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
