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
 * @brief Runs an ASIO input through the Tube Screamer, the JCM800 and an optional cabinet.
 * @author Mustafa Garip
 */

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "SushiDSP/cabinet/cabinet_node.hpp"
#include "SushiDSP/core/rig_graph.hpp"
#include "SushiDSP/device/asio_audio_device.hpp"
#include "SushiDSP/audio_format/channel_selection.hpp"
#include "SushiDSP/io/ir_loader.hpp"
#include "SushiDSP/jcm800/jcm800.hpp"
#include "SushiDSP/log/async_log_sink.hpp"
#include "SushiDSP/log/default_logger.hpp"
#include "SushiDSP/log/scoped_log_sink.hpp"
#include "SushiDSP/log/stderr_log_sink.hpp"
#include "SushiDSP/tubescreamer/tube_screamer.hpp"

namespace
{
    /// @brief Parses "--name=value" style flags. Returns true and fills value if prefix matches.
    bool match_flag(const std::string& arg, const char* prefix, std::string& value)
    {
        const std::size_t prefix_len = std::strlen(prefix);
        if (arg.compare(0, prefix_len, prefix) == 0)
        {
            value = arg.substr(prefix_len);
            return true;
        }
        return false;
    }
} // namespace

int main(int argc, char** argv)
{
    // The guard flushes the asynchronous sink on every return path of main.
    const SushiDSP::ScopedLogSink log_sink(
        SushiDSP::default_logger(),
        std::make_shared<SushiDSP::AsyncLogSink>(std::make_shared<SushiDSP::StderrLogSink>(),
                                                 SushiDSP::K_DEFAULT_LOG_QUEUE_CAPACITY));

    const std::vector<std::string> drivers = SushiDSP::AsioAudioDevice::enumerate_drivers();
    if (drivers.empty())
    {
        std::fprintf(stderr, "No ASIO drivers are registered on this machine.\n");
        return 1;
    }

    if (argc < 2)
    {
        std::printf(
            "Usage: %s <asio-driver-name> [--input=L|R] [--output=L|R|stereo] [cabinet-ir.wav]\n"
            "Available drivers:\n",
            argv[0]);
        for (const std::string& name : drivers)
        {
            std::printf("  %s\n", name.c_str());
        }
        return 1;
    }

    SushiDSP::ChannelSelection input_selection = SushiDSP::ChannelSelection::Left;
    SushiDSP::ChannelSelection output_selection = SushiDSP::ChannelSelection::Stereo;
    std::vector<std::string> positional;

    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        std::string value;
        try
        {
            if (match_flag(arg, "--input=", value))
            {
                input_selection = SushiDSP::parse_channel_selection(value, "--input");
            }
            else if (match_flag(arg, "--output=", value))
            {
                output_selection = SushiDSP::parse_channel_selection(value, "--output");
            }
            else
            {
                positional.push_back(arg);
            }
        }
        catch (const std::exception& error)
        {
            std::fprintf(stderr, "%s\n", error.what());
            return 1;
        }
    }

    if (positional.empty())
    {
        std::fprintf(stderr, "Missing <asio-driver-name>.\n");
        return 1;
    }

    const std::string driver_name = positional[0];
    const std::string ir_path = positional.size() >= 2 ? positional[1] : std::string();

    // Both channels open in each direction whatever was selected; see applications/host_asio/README.md.
    SushiDSP::DeviceFormat requested{48000.0, 256, 2, 2};

    std::unique_ptr<SushiDSP::AsioAudioDevice> device;
    try
    {
        device = std::make_unique<SushiDSP::AsioAudioDevice>(driver_name, requested);
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Failed to open ASIO driver '%s': %s\n", driver_name.c_str(),
                     error.what());
        return 1;
    }

    const SushiDSP::DeviceFormat actual = device->format();
    std::printf("Opened '%s': %.0f Hz, %u frames/block, %u input / %u output channel(s)\n",
                driver_name.c_str(), actual.sample_rate, actual.block_size, actual.input_channels,
                actual.output_channels);
    std::printf("Reading input %s, writing output %s\n",
                SushiDSP::channel_selection_label(input_selection),
                SushiDSP::channel_selection_label(output_selection));

    std::vector<float> cabinet_ir;
    if (!ir_path.empty())
    {
        try
        {
            const SushiDSP::Ir ir = SushiDSP::IrLoader::load(ir_path, actual.sample_rate);
            cabinet_ir = ir.samples;
            std::printf("Loaded cabinet IR '%s' (%zu samples @ %.0f Hz)\n", ir_path.c_str(),
                        cabinet_ir.size(), ir.sample_rate);
        }
        catch (const std::exception& error)
        {
            std::fprintf(stderr, "Failed to load cabinet IR '%s': %s\n", ir_path.c_str(),
                         error.what());
            return 1;
        }
    }

    SushiDSP::RigGraph rig(actual.block_size, 4);

    // Hardcoded demo settings: there is no rig-editor UI yet.
    auto tube_screamer = std::make_unique<SushiDSP::TubeScreamer>();
    tube_screamer->set_drive(0.5f);
    tube_screamer->set_tone(0.5f);
    tube_screamer->set_level(0.0f); // Level is now in dB; 0 dB is the pedal's unity-ish output

    auto jcm800 = std::make_unique<SushiDSP::Jcm800>();
    jcm800->set_preamp(0.6f);
    jcm800->set_master(0.0f);
    jcm800->set_bass(0.5f);
    jcm800->set_middle(0.5f);
    jcm800->set_treble(0.6f);
    jcm800->set_presence(0.5f);

    // With no upstream edge, tube_screamer_handle takes live input; see applications/host_asio/README.md.
    const SushiDSP::NodeHandle tube_screamer_handle = rig.add_node(std::move(tube_screamer));
    const SushiDSP::NodeHandle jcm800_handle = rig.add_node(std::move(jcm800));
    rig.connect(tube_screamer_handle, jcm800_handle);

    // The cabinet node joins the chain only with an IR; see applications/host_asio/README.md.
    SushiDSP::NodeHandle cabinet_handle{};
    const bool has_cabinet = !cabinet_ir.empty();
    if (has_cabinet)
    {
        auto cabinet = std::make_unique<SushiDSP::CabinetNode>();
        cabinet_handle = rig.add_node(std::move(cabinet));
        rig.connect(jcm800_handle, cabinet_handle);
    }

    // Rig width follows the device, capped at RigGraph::K_MAX_CHANNELS; the graph fans out mono.
    const std::uint32_t rig_channels =
        std::min(std::max(1u, actual.output_channels), SushiDSP::RigGraph::K_MAX_CHANNELS);
    SushiDSP::PrepareInfo info{actual.sample_rate, actual.block_size, rig_channels};
    rig.compile(info);
    std::printf("Rig latency: %u sample(s)\n", rig.latency_samples());

    if (has_cabinet)
    {
        auto* const cabinet_node = static_cast<SushiDSP::CabinetNode*>(rig.node(cabinet_handle));
        if (!cabinet_node->set_cabinet(SushiDSP::Ir{cabinet_ir, actual.sample_rate}))
        {
            std::fprintf(stderr, "Cabinet IR did not install (rate mismatch or empty IR).\n");
        }
    }

    const std::size_t input_index = SushiDSP::channel_selection_input_index(input_selection);
    const std::uint32_t device_output_channels = actual.output_channels;
    if (!SushiDSP::rig_output_adapts_to_device(rig_channels, device_output_channels))
    {
        std::fprintf(stderr, "A %u-channel rig cannot be written to %u device output channel(s).\n",
                     rig_channels, device_output_channels);
        return 1;
    }

    // rig_output and its pointer arrays are sized here, off the audio thread.
    const std::size_t block = actual.block_size;
    std::vector<float> rig_output(static_cast<std::size_t>(rig_channels) * block, 0.0f);
    std::vector<float*> rig_output_pointers(rig_channels);
    std::vector<const float*> rig_output_const_pointers(rig_channels);
    for (std::uint32_t channel = 0; channel < rig_channels; ++channel)
    {
        rig_output_pointers[channel] = rig_output.data() + channel * block;
        rig_output_const_pointers[channel] = rig_output_pointers[channel];
    }

    device->start(
        [&rig, &rig_output_pointers, &rig_output_const_pointers, rig_channels, input_index,
         output_selection, device_output_channels](const float* const* inputs,
                                                   float* const* outputs, std::uint32_t frames)
        {
            const float* selected_input[1] = {inputs[input_index]};
            SushiDSP::ProcessContext context{rig_output_pointers.data(), selected_input, frames,
                                             0.0};
            rig.process(context);

            SushiDSP::write_rig_output_to_device(rig_output_const_pointers.data(), rig_channels,
                                                 outputs, device_output_channels, frames,
                                                 output_selection);
        });

    std::printf("Playing live through TubeScreamer -> JCM800. Press Enter to stop.\n");
    std::getchar();

    device->stop();
    std::printf("Stopped.\n");

    return 0;
}
