/**************************************************************************/
/* test_preset.cpp                                                        */
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

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Preset.hpp"
#include "RigController.hpp"
#include "SushiDSP/node_registry/NodeRegistry.hpp"

namespace
{
    std::string temp_preset_path()
    {
        return std::string(::testing::TempDir()) + "sushidsp_test_preset.json";
    }

    void append_u32_le(std::vector<std::uint8_t>& buffer, std::uint32_t value)
    {
        for (int byte = 0; byte < 4; ++byte)
        {
            buffer.push_back(static_cast<std::uint8_t>((value >> (8 * byte)) & 0xFF));
        }
    }

    void append_u16_le(std::vector<std::uint8_t>& buffer, std::uint16_t value)
    {
        buffer.push_back(static_cast<std::uint8_t>(value & 0xFF));
        buffer.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
    }

    void append_tag(std::vector<std::uint8_t>& buffer, const char* tag)
    {
        buffer.insert(buffer.end(), tag, tag + 4);
    }

    // A minimal mono 16-bit PCM impulse, just enough for IrLoader to produce a valid Ir.
    std::string write_temp_impulse_wav()
    {
        const std::uint32_t sample_rate = 48000;
        std::vector<std::int16_t> samples(64, 0);
        samples[0] = 16384;

        const std::uint32_t data_bytes = static_cast<std::uint32_t>(samples.size() * sizeof(std::int16_t));
        std::vector<std::uint8_t> bytes;
        append_tag(bytes, "RIFF");
        append_u32_le(bytes, 36 + data_bytes);
        append_tag(bytes, "WAVE");
        append_tag(bytes, "fmt ");
        append_u32_le(bytes, 16);
        append_u16_le(bytes, 1);
        append_u16_le(bytes, 1);
        append_u32_le(bytes, sample_rate);
        append_u32_le(bytes, sample_rate * 2);
        append_u16_le(bytes, 2);
        append_u16_le(bytes, 16);
        append_tag(bytes, "data");
        append_u32_le(bytes, data_bytes);
        for (std::int16_t sample : samples)
        {
            append_u16_le(bytes, static_cast<std::uint16_t>(sample));
        }

        const std::string path = std::string(::testing::TempDir()) + "sushidsp_test_cabinet.wav";
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        return path;
    }
}

TEST(Unit_Dsp, PresetSaveLoadRoundTrip)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    RigController writer(info);
    writer.append("sushidsp.tubescreamer");
    writer.set_parameter(0, "drive", 0.33f);
    writer.set_parameter(0, "tone", 0.66f);
    writer.set_parameter(0, "level", 0.77f);
    writer.append("sushidsp.jcm800");
    writer.set_parameter(1, "preamp", 0.11f);
    writer.set_parameter(1, "master", 0.22f);

    const std::string path = temp_preset_path();
    save_preset(writer, path);

    RigController reader(info);
    load_preset(reader, path);

    const auto ids = reader.chain_ids();
    ASSERT_EQ(ids.size(), 2u);
    EXPECT_EQ(ids[0], "sushidsp.tubescreamer");
    EXPECT_EQ(ids[1], "sushidsp.jcm800");
    EXPECT_FLOAT_EQ(reader.get_parameter(0, "drive"), 0.33f);
    EXPECT_FLOAT_EQ(reader.get_parameter(0, "tone"), 0.66f);
    EXPECT_FLOAT_EQ(reader.get_parameter(0, "level"), 0.77f);
    EXPECT_FLOAT_EQ(reader.get_parameter(1, "preamp"), 0.11f);
    EXPECT_FLOAT_EQ(reader.get_parameter(1, "master"), 0.22f);

    std::remove(path.c_str());
}

TEST(Unit_Dsp, PresetSaveLoadRoundTripPreservesDescriptorDefaultsWhenUntouched)
{
    // Regression test for the Task 7 review finding: RigController::insert() used to leave a
    // freshly-added slot's parameter map empty, so get_parameter() (which save_preset() calls)
    // returned 0.0f for every parameter nobody had explicitly set - even though the live node,
    // and the UI sliders reading from it, showed the real descriptor defaults. Saving a preset
    // without ever touching a slider silently zeroed every parameter.
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    RigController writer(info);
    writer.append("sushidsp.tubescreamer");
    writer.append("sushidsp.jcm800");
    // Deliberately never call set_parameter() here.

    const std::string path = temp_preset_path();
    save_preset(writer, path);

    RigController reader(info);
    load_preset(reader, path);

    const auto ids = reader.chain_ids();
    ASSERT_EQ(ids.size(), 2u);

    for (std::size_t i = 0; i < ids.size(); ++i)
    {
        for (const auto& entry : SushiDSP::node_registry())
        {
            if (ids[i] != entry.descriptor->id)
            {
                continue;
            }
            for (std::size_t p = 0; p < entry.descriptor->parameter_count; ++p)
            {
                const auto& param = entry.descriptor->parameters[p];
                EXPECT_FLOAT_EQ(reader.get_parameter(i, param.id), param.default_value)
                    << "node " << ids[i] << " parameter " << param.id;
            }
            break;
        }
    }

    std::remove(path.c_str());
}

TEST(Unit_Dsp, PresetRoundTripsCabinetPath)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    const std::string ir_path = write_temp_impulse_wav();

    RigController writer(info);
    writer.append("sushidsp.tubescreamer");
    writer.append("sushidsp.jcm800");
    ASSERT_TRUE(writer.set_cabinet(1, ir_path));

    const std::string path = temp_preset_path();
    save_preset(writer, path);

    RigController reader(info);
    load_preset(reader, path);

    const auto paths = reader.chain_cabinet_paths();
    ASSERT_EQ(paths.size(), 2u);
    EXPECT_TRUE(paths[0].empty());
    EXPECT_EQ(paths[1], ir_path);

    std::remove(path.c_str());
    std::remove(ir_path.c_str());
}

TEST(Unit_Dsp, PresetWithoutCabinetPathLoadsWithNone)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    RigController writer(info);
    writer.append("sushidsp.jcm800");

    const std::string path = temp_preset_path();
    save_preset(writer, path);

    RigController reader(info);
    load_preset(reader, path);

    const auto paths = reader.chain_cabinet_paths();
    ASSERT_EQ(paths.size(), 1u);
    EXPECT_TRUE(paths[0].empty());

    std::remove(path.c_str());
}
