/**************************************************************************/
/* test_cabinet_node.cpp                                                 */
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
/*                                                                        */
/*     http://www.apache.org/licenses/LICENSE-2.0                         */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

#include <cmath>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "SushiDSP/cabinet/CabinetNode.hpp"

namespace
{
    std::vector<float> make_identity_impulse_response()
    {
        return std::vector<float>{ 1.0f };
    }
}

TEST(Unit_Dsp, CabinetNodeIsAPassthroughWithNoCabinetInstalled)
{
    SushiDSP::CabinetNode node;
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    node.prepare(info);

    std::vector<float> input(256);
    for (std::uint32_t i = 0; i < input.size(); ++i)
    {
        input[i] = std::sin(static_cast<float>(i) * 0.1f);
    }
    std::vector<float> output(256, 0.0f);
    const float* inputs[1] = { input.data() };
    float* outputs[1] = { output.data() };
    SushiDSP::ProcessContext context{ outputs, inputs, static_cast<std::uint32_t>(input.size()), 0.0 };

    node.process(context);

    for (std::size_t i = 0; i < input.size(); ++i)
    {
        EXPECT_FLOAT_EQ(output[i], input[i]);
    }
    EXPECT_FALSE(node.has_cabinet());
}

TEST(Unit_Dsp, CabinetNodeInstallsAndAppliesAnImpulseResponse)
{
    SushiDSP::CabinetNode node;
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    node.prepare(info);

    EXPECT_FALSE(node.has_cabinet());
    EXPECT_TRUE(node.set_cabinet(SushiDSP::Ir{ make_identity_impulse_response(), 48000.0 }));
    EXPECT_TRUE(node.has_cabinet());
    node.reclaim_cabinets();

    std::vector<float> input(256, 1.0f);
    std::vector<float> output(256, 0.0f);
    const float* inputs[1] = { input.data() };
    float* outputs[1] = { output.data() };
    SushiDSP::ProcessContext context{ outputs, inputs, static_cast<std::uint32_t>(input.size()), 0.0 };

    node.process(context);

    // An identity IR makes the cabinet a pure delay of one partition; every sample that has
    // seen the delayed input by now must reproduce it exactly.
    const std::uint32_t latency = node.latency_samples();
    for (std::uint32_t i = latency; i < input.size(); ++i)
    {
        EXPECT_FLOAT_EQ(output[i], 1.0f);
    }
}

TEST(Unit_Dsp, CabinetNodeRejectsAnImpulseResponseAtTheWrongSampleRate)
{
    SushiDSP::CabinetNode node;
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    node.prepare(info);

    EXPECT_FALSE(node.set_cabinet(SushiDSP::Ir{ make_identity_impulse_response(), 44100.0 }));
    EXPECT_FALSE(node.has_cabinet());
}

TEST(Unit_Dsp, CabinetNodeRejectsAnImpulseResponseBeforePrepare)
{
    SushiDSP::CabinetNode node;

    EXPECT_FALSE(node.set_cabinet(SushiDSP::Ir{ make_identity_impulse_response(), 48000.0 }));
    EXPECT_FALSE(node.has_cabinet());
}

TEST(Unit_Dsp, CabinetNodeDescriptorMatchesTypeId)
{
    const SushiDSP::NodeDescriptor& descriptor = SushiDSP::cabinet_descriptor();
    EXPECT_STREQ(descriptor.id, "sushidsp.cabinet");
    EXPECT_EQ(descriptor.ports.input_channels, 1u);
    EXPECT_EQ(descriptor.ports.output_channels, 1u);
    EXPECT_EQ(descriptor.parameter_count, 0u);

    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    auto node = descriptor.make(info);
    ASSERT_NE(node, nullptr);
    EXPECT_STREQ(node->type_id(), "sushidsp.cabinet");
}
