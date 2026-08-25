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

#include <cstdio>
#include <string>

#include <gtest/gtest.h>

#include "Preset.hpp"
#include "RigController.hpp"

namespace
{
    std::string temp_preset_path()
    {
        return std::string(::testing::TempDir()) + "sushidsp_test_preset.json";
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
