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
#include <stdexcept>
#include <string>
#include <vector>

#include "SushiDSP/core/RigGraph.hpp"
#include "SushiDSP/device/AsioAudioDevice.hpp"
#include "SushiDSP/device/ChannelSelection.hpp"
#include "SushiDSP/io/IrLoader.hpp"
#include "SushiDSP/jcm800/Jcm800.hpp"
#include "SushiDSP/tubescreamer/TubeScreamer.hpp"

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
}

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

    // Always open both channels, regardless of selection, so --input=R (guitar wired to the
    // right input) and --output=stereo both have a real ASIO channel to read from/write to.
    SushiDSP::DeviceFormat requested{ 48000.0, 256, 2, 2 };

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
    std::printf(
        "Reading input %s, writing output %s\n",
        SushiDSP::channel_selection_label(input_selection),
        SushiDSP::channel_selection_label(output_selection));

    std::vector<float> cabinet_ir;
    if (!ir_path.empty())
    {
        try
        {
            const SushiDSP::Ir ir = SushiDSP::IrLoader::load(ir_path, actual.sample_rate);
            cabinet_ir = ir.samples;
            std::printf("Loaded cabinet IR '%s' (%zu samples @ %.0f Hz)\n",
                ir_path.c_str(), cabinet_ir.size(), ir.sample_rate);
        }
        catch (const std::exception& error)
        {
            std::fprintf(stderr, "Failed to load cabinet IR '%s': %s\n", ir_path.c_str(), error.what());
            return 1;
        }
    }

    SushiDSP::RigGraph rig(actual.block_size, 4);

    // Hardcoded demo settings: there is no rig-editor UI yet.
    auto tube_screamer = std::make_unique<SushiDSP::TubeScreamer>();
    tube_screamer->set_drive(0.5f);
    tube_screamer->set_tone(0.5f);
    tube_screamer->set_level(0.0f);   // Level is now in dB; 0 dB is the pedal's unity-ish output

    auto jcm800 = std::make_unique<SushiDSP::Jcm800>(cabinet_ir);
    jcm800->set_preamp(0.6f);
    jcm800->set_master(0.0f);
    jcm800->set_bass(0.5f);
    jcm800->set_middle(0.5f);
    jcm800->set_treble(0.6f);
    jcm800->set_presence(0.5f);

    // No SineToneNode: tube_screamer_handle has no upstream edge, so RigGraph feeds it the
    // device's own live input directly (Task 1's root-node passthrough).
    const SushiDSP::NodeHandle tube_screamer_handle = rig.add_node(std::move(tube_screamer));
    const SushiDSP::NodeHandle jcm800_handle = rig.add_node(std::move(jcm800));
    rig.connect(tube_screamer_handle, jcm800_handle);

    // The rig itself stays mono (1 in, 1 out) - channel selection only decides which physical
    // ASIO channel feeds it and which physical channel(s) its output reaches.
    SushiDSP::PrepareInfo info{ actual.sample_rate, actual.block_size, 1 };
    rig.compile(info);
    std::printf("Rig latency: %u sample(s)\n", rig.latency_samples());

    const std::size_t input_index = SushiDSP::channel_selection_input_index(input_selection);

    // RigGraph::process() only ever writes to outputs[0], so the rig always renders into this
    // private mono scratch buffer; the callback below then fans it out to whichever physical
    // output(s) --output selected. Sized once, up front, off the audio thread: the callback
    // itself must stay allocation-free.
    std::vector<float> mono_output(actual.block_size, 0.0f);

    device->start([&rig, &mono_output, input_index, output_selection](const float* const* inputs, float* const* outputs, std::uint32_t frames)
    {
        const float* selected_input[1] = { inputs[input_index] };
        float* mono_output_ptr[1] = { mono_output.data() };
        SushiDSP::ProcessContext context{ mono_output_ptr, selected_input, frames, 0.0 };
        rig.process(context);

        SushiDSP::fan_out_mono_to_stereo(mono_output.data(), outputs, frames, output_selection);
    });

    std::printf("Playing live through TubeScreamer -> JCM800. Press Enter to stop.\n");
    std::getchar();

    device->stop();
    std::printf("Stopped.\n");

    return 0;
}
