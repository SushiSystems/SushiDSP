/****************************************************************/
/* main.cpp                                                     */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file main.cpp
 * @brief Runs a 440 Hz tone through the Tube Screamer and the JCM800 for three seconds.
 * @author Mustafa Garip
 */

#include <chrono>
#include <cstdio>
#include <memory>
#include <thread>

#include "SineToneNode.hpp"
#include "SushiDSP/core/RigGraph.hpp"
#include "SushiDSP/device/Sdl2AudioDevice.hpp"
#include "SushiDSP/jcm800/Jcm800.hpp"
#include "SushiDSP/tubescreamer/TubeScreamer.hpp"

int main()
{
    SushiDSP::DeviceFormat requested{ 48000.0, 256, 0, 1 };

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
        "Opened audio device: %.0f Hz, %u frames/block, %u output channel(s)\n",
        actual.sample_rate, actual.block_size, actual.output_channels);

    SushiDSP::RigGraph rig(actual.block_size, 4);

    // The node controls are set once here; apps/host/README.md says why.
    auto tube_screamer = std::make_unique<SushiDSP::TubeScreamer>();
    tube_screamer->set_drive(0.7f);
    tube_screamer->set_tone(0.5f);
    tube_screamer->set_level(0.0f);   // Level is now in dB; 0 dB is the pedal's unity-ish output

    // The rig ends at the amplifier with no cabinet node; see apps/host/README.md.
    auto jcm800 = std::make_unique<SushiDSP::Jcm800>();
    jcm800->set_preamp(0.7f);
    jcm800->set_master(0.0f);
    jcm800->set_bass(0.5f);
    jcm800->set_middle(0.4f);
    jcm800->set_treble(0.7f);
    jcm800->set_presence(0.6f);

    const SushiDSP::NodeHandle source_handle =
        rig.add_node(std::make_unique<SushiDSP::SineToneNode>(440.0f));
    const SushiDSP::NodeHandle tube_screamer_handle = rig.add_node(std::move(tube_screamer));
    const SushiDSP::NodeHandle jcm800_handle = rig.add_node(std::move(jcm800));
    rig.connect(source_handle, tube_screamer_handle);
    rig.connect(tube_screamer_handle, jcm800_handle);

    SushiDSP::PrepareInfo info{ actual.sample_rate, actual.block_size, actual.output_channels };
    rig.compile(info);

    device->start([&rig](const float* const* inputs, float* const* outputs, std::uint32_t frames)
    {
        SushiDSP::ProcessContext context{ outputs, inputs, frames, 0.0 };
        rig.process(context);
    });

    std::printf(
        "Playing a 440 Hz test tone through the Tube Screamer into the JCM800 for 3 seconds...\n");
    std::this_thread::sleep_for(std::chrono::seconds(3));

    device->stop();
    std::printf("Stopped.\n");

    return 0;
}
