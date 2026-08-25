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
    rig.add_node(std::make_unique<SushiDSP::SineToneNode>(440.0f));

    SushiDSP::PrepareInfo info{ actual.sample_rate, actual.block_size, actual.channels };
    rig.compile(info);

    device->start([&rig](float* const* outputs, std::uint32_t frames)
    {
        SushiDSP::ProcessContext context{ outputs, nullptr, frames, 0.0 };
        rig.process(context);
    });

    std::printf("Playing a 440 Hz test tone for 3 seconds...\n");
    std::this_thread::sleep_for(std::chrono::seconds(3));

    device->stop();
    std::printf("Stopped.\n");

    return 0;
}
