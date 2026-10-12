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
 * @brief Runs a 440 Hz tone through the Tube Screamer and the JCM800 for three seconds.
 * @author Mustafa Garip
 */

#include <chrono>
#include <cstdio>
#include <memory>
#include <thread>

#include "sine_tone_node.hpp"
#include "SushiDSP/core/rig_graph.hpp"
#include "SushiDSP/device/sdl2_audio_device.hpp"
#include "SushiDSP/jcm800/jcm800.hpp"
#include "SushiDSP/log/async_log_sink.hpp"
#include "SushiDSP/log/default_logger.hpp"
#include "SushiDSP/log/scoped_log_sink.hpp"
#include "SushiDSP/log/stderr_log_sink.hpp"
#include "SushiDSP/tubescreamer/tube_screamer.hpp"

int main()
{
    // The guard flushes the asynchronous sink on every return path of main.
    const SushiDSP::ScopedLogSink log_sink(
        SushiDSP::default_logger(),
        std::make_shared<SushiDSP::AsyncLogSink>(std::make_shared<SushiDSP::StderrLogSink>(),
                                                 SushiDSP::K_DEFAULT_LOG_QUEUE_CAPACITY));

    SushiDSP::DeviceFormat requested{48000.0, 256, 0, 1};

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
    std::printf("Opened audio device: %.0f Hz, %u frames/block, %u output channel(s)\n",
                actual.sample_rate, actual.block_size, actual.output_channels);

    SushiDSP::RigGraph rig(actual.block_size, 4);

    // The node controls are set once here; applications/host/README.md says why.
    auto tube_screamer = std::make_unique<SushiDSP::TubeScreamer>();
    tube_screamer->set_drive(0.7f);
    tube_screamer->set_tone(0.5f);
    tube_screamer->set_level(0.0f); // Level is now in dB; 0 dB is the pedal's unity-ish output

    // The rig ends at the amplifier with no cabinet node; see applications/host/README.md.
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

    SushiDSP::PrepareInfo info{actual.sample_rate, actual.block_size, actual.output_channels};
    rig.compile(info);

    device->start(
        [&rig](const float* const* inputs, float* const* outputs, std::uint32_t frames)
        {
            SushiDSP::ProcessContext context{outputs, inputs, frames, 0.0};
            rig.process(context);
        });

    std::printf(
        "Playing a 440 Hz test tone through the Tube Screamer into the JCM800 for 3 seconds...\n");
    std::this_thread::sleep_for(std::chrono::seconds(3));

    device->stop();
    std::printf("Stopped.\n");

    return 0;
}
