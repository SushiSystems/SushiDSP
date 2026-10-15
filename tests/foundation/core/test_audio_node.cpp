/**************************************************************************/
/* test_audio_node.cpp                                                    */
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
 * @file test_audio_node.cpp
 * @brief Tests the behaviour AudioNode gives a node that overrides only what it must.
 * @author Mustafa Garip
 */

#include <cstring>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/core/audio_node.hpp>

namespace
{
    /// @brief Declares a node that overrides the pure virtual methods alone.
    class MinimalNode final : public SushiDSP::AudioNode
    {
        public:
            void prepare(const SushiDSP::PrepareInfo&) override
            {

            }

            void process(SushiDSP::ProcessContext&) override
            {

            }

            SushiDSP::PortLayout ports() const override
            {
                return {1, 2};
            }

            float get_parameter(const char*) const override
            {
                return 0.0f;
            }

            void set_parameter_for_control_thread(const char*, float) override
            {

            }

            void reset() override
            {

            }
    };
} // namespace

TEST(Small_AudioNode, DeclaresOneOutputPortByDefault)
{
    const MinimalNode node;
    EXPECT_EQ(node.output_port_count(), 1u);
}

TEST(Small_AudioNode, PortZeroTakesItsWidthFromPorts)
{
    const MinimalNode node;
    EXPECT_EQ(node.output_port_channels(0), 2u);
}

TEST(Small_AudioNode, PortsPastZeroHaveNoChannelsByDefault)
{
    const MinimalNode node;
    EXPECT_EQ(node.output_port_channels(1), 0u);
    EXPECT_EQ(node.output_port_channels(7), 0u);
}

TEST(Small_AudioNode, ReportsNoLatencyByDefault)
{
    const MinimalNode node;
    EXPECT_EQ(node.latency_samples(), 0u);
}

TEST(Small_AudioNode, ReportsAnEmptyTypeIDByDefault)
{
    const MinimalNode node;
    ASSERT_NE(node.type_id(), nullptr);
    EXPECT_EQ(std::strlen(node.type_id()), 0u);
}

TEST(Small_AudioNode, IgnoresAParameterWrittenByIndexByDefault)
{
    MinimalNode node;
    node.set_parameter_for_audio_thread(0, 0.75f);
    node.set_parameter_for_audio_thread(65535, 1.0f);
    EXPECT_FLOAT_EQ(node.get_parameter("anything"), 0.0f);
}
