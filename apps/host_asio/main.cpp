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
#include <string>
#include <vector>

#include "SushiDSP/core/RigGraph.hpp"
#include "SushiDSP/device/AsioAudioDevice.hpp"
#include "SushiDSP/jcm800/Jcm800.hpp"
#include "SushiDSP/tubescreamer/TubeScreamer.hpp"

int main(int argc, char** argv)
{
    const std::vector<std::string> drivers = SushiDSP::AsioAudioDevice::enumerate_drivers();
    if (drivers.empty())
    {
        std::fprintf(stderr, "No ASIO drivers are registered on this machine.\n");
        return 1;
    }

    if (argc < 2)
    {
        std::printf("Usage: %s <asio-driver-name>\nAvailable drivers:\n", argv[0]);
        for (const std::string& name : drivers)
        {
            std::printf("  %s\n", name.c_str());
        }
        return 1;
    }

    const std::string driver_name = argv[1];
    SushiDSP::DeviceFormat requested{ 48000.0, 256, 1, 1 };

    std::unique_ptr<SushiDSP::AsioAudioDevice> device;
    try
    {
        device = std::make_unique<SushiDSP::AsioAudioDevice>(driver_name, requested);
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Failed to open ASIO driver '%s': %s\n", driver_name.c_str(), error.what());
        return 1;
    }

    const SushiDSP::DeviceFormat actual = device->format();
    std::printf(
        "Opened '%s': %.0f Hz, %u frames/block, %u input / %u output channel(s)\n",
        driver_name.c_str(), actual.sample_rate, actual.block_size,
        actual.input_channels, actual.output_channels);

    SushiDSP::RigGraph rig(actual.block_size, 4);

    // Hardcoded demo settings: there is no rig-editor UI yet.
    auto tube_screamer = std::make_unique<SushiDSP::TubeScreamer>();
    tube_screamer->set_drive(0.5f);
    tube_screamer->set_tone(0.5f);
    tube_screamer->set_level(0.8f);

    auto jcm800 = std::make_unique<SushiDSP::Jcm800>();
    jcm800->set_preamp(0.6f);
    jcm800->set_master(0.6f);
    jcm800->set_bass(0.5f);
    jcm800->set_middle(0.5f);
    jcm800->set_treble(0.6f);
    jcm800->set_presence(0.5f);

    // No SineToneNode: tube_screamer_handle has no upstream edge, so RigGraph feeds it the
    // device's own live input directly (Task 1's root-node passthrough).
    const SushiDSP::NodeHandle tube_screamer_handle = rig.add_node(std::move(tube_screamer));
    const SushiDSP::NodeHandle jcm800_handle = rig.add_node(std::move(jcm800));
    rig.connect(tube_screamer_handle, jcm800_handle);

    SushiDSP::PrepareInfo info{ actual.sample_rate, actual.block_size, actual.output_channels };
    rig.compile(info);

    device->start([&rig](const float* const* inputs, float* const* outputs, std::uint32_t frames)
    {
        SushiDSP::ProcessContext context{ outputs, inputs, frames, 0.0 };
        rig.process(context);
    });

    std::printf("Playing live through TubeScreamer -> JCM800. Press Enter to stop.\n");
    std::getchar();

    device->stop();
    std::printf("Stopped.\n");

    return 0;
}
