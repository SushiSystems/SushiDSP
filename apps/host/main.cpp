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

#include <chrono>
#include <cstdio>
#include <memory>
#include <thread>

#include "SineToneNode.hpp"
#include "SushiDSP/core/RigGraph.hpp"
#include "SushiDSP/device/Sdl2AudioDevice.hpp"
#include "SushiDSP/tubescreamer/TubeScreamer.hpp"

int main()
{
    SushiDSP::DeviceFormat requested{ 48000.0, 256, 1 };

    std::unique_ptr<SushiDSP::Sdl2AudioDevice> device;
    try
    {
        device = std::make_unique<SushiDSP::Sdl2AudioDevice>(requested);
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Failed to open audio device: %s\n", error.what());
        return 1;
    }

    const SushiDSP::DeviceFormat actual = device->format();
    std::printf(
        "Opened audio device: %.0f Hz, %u frames/block, %u channel(s)\n",
        actual.sample_rate, actual.block_size, actual.channels);

    SushiDSP::RigGraph rig(actual.block_size, 4);

    // Hardcoded demo settings: there is no rig-editor UI yet, so the Tube Screamer's controls
    // are set once here rather than driven live.
    auto tube_screamer = std::make_unique<SushiDSP::TubeScreamer>();
    tube_screamer->set_drive(0.7f);
    tube_screamer->set_tone(0.5f);
    tube_screamer->set_level(0.8f);

    const SushiDSP::NodeHandle source_handle =
        rig.add_node(std::make_unique<SushiDSP::SineToneNode>(440.0f));
    const SushiDSP::NodeHandle tube_screamer_handle = rig.add_node(std::move(tube_screamer));
    rig.connect(source_handle, tube_screamer_handle);

    SushiDSP::PrepareInfo info{ actual.sample_rate, actual.block_size, actual.channels };
    rig.compile(info);

    device->start([&rig](float* const* outputs, std::uint32_t frames)
    {
        SushiDSP::ProcessContext context{ outputs, nullptr, frames, 0.0 };
        rig.process(context);
    });

    std::printf("Playing a 440 Hz test tone through the Tube Screamer for 3 seconds...\n");
    std::this_thread::sleep_for(std::chrono::seconds(3));

    device->stop();
    std::printf("Stopped.\n");

    return 0;
}
