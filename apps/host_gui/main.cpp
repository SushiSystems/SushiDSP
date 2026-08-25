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

#if SUSHIDSP_HAVE_ASIO
#include "SushiDSP/device/AsioAudioDevice.hpp"
#endif

namespace
{
    /// @brief Builds a one-node starter rig (TubeScreamer only) fully compiled and ready to
    /// hand to RtGraphSlot::publish(). Task 6 replaces the hardcoded catalog id with whatever
    /// the rack strip actually contains.
    std::unique_ptr<SushiDSP::RigGraph> build_starter_rig(const SushiDSP::PrepareInfo& info)
    {
        auto graph = std::make_unique<SushiDSP::RigGraph>(info.max_block, 4);
        auto node = SushiDSP::make_node("sushidsp.tubescreamer", info);
        graph->add_node(std::move(node));
        graph->compile(info);
        return graph;
    }
}

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

    SushiDSP::RtGraphSlot graph_slot;
    SushiDSP::PrepareInfo info{ actual.sample_rate, actual.block_size, actual.output_channels };
    graph_slot.publish(build_starter_rig(info));

    // Sdl2AudioDevice is playback-only (format().input_channels == 0) and always hands the
    // callback a nullptr inputs array - a valid IAudioDevice contract (see IAudioDevice.hpp).
    // TubeScreamer, like any effect node, unconditionally reads its input channel, so the root
    // node of a rig with no live capture needs a real (silent) buffer rather than nullptr. This
    // silence buffer is sized once up front (non-RT) and only ever read on the audio thread, so
    // feeding it in place of a missing capture device stays allocation-free and RT-safe.
    std::vector<float> silence(actual.block_size, 0.0f);

    device->start([&graph_slot, &silence](const float* const* inputs, float* const* outputs, std::uint32_t frames)
    {
        SushiDSP::RigGraph* active = graph_slot.acquire_for_audio_thread();
        const float* silence_channel = silence.data();
        const float* const* effective_inputs = (inputs != nullptr) ? inputs : &silence_channel;
        SushiDSP::ProcessContext context{ outputs, effective_inputs, frames, 0.0 };
        if (active != nullptr)
        {
            active->process(context);
        }
    });

    // Proves RtGraphSlot's swap path, not just its initial publish(): a few seconds in, build a
    // second independently-compiled one-node graph and publish() it once. The audio thread picks
    // it up via acquire_for_audio_thread() on its very next block with no locking and no
    // glitch; the old graph is freed on this (non-RT) thread only after reclaim() confirms the
    // audio thread has moved off it.
    bool swapped = false;
    constexpr double kSwapAfterSeconds = 3.0;

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        graph_slot.reclaim();

        if (!swapped && ImGui::GetTime() >= kSwapAfterSeconds)
        {
            graph_slot.publish(build_starter_rig(info));
            swapped = true;
            std::fprintf(stdout, "Swapped to second graph\n");
            std::fflush(stdout);
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("SushiDSP Rig");
        ImGui::Text("Skeleton window - rack strip and gear panel land in Task 6.");
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
