/**************************************************************************/
/* test_profiled_node.cpp                                                 */
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
 * @file test_profiled_node.cpp
 * @brief Tests that profile_node() runs one block per period and hands back the record.
 * @author Mustafa Garip
 */

#include <cstdint>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/benchmark_harness/profiled_node.hpp>
#include <SushiDSP/foundation/profile/profile.hpp>

namespace
{
    /// @brief Declares a node that counts its blocks and zones each one.
    class CountingNode final : public SushiDSP::AudioNode
    {
        public:
            void prepare(const SushiDSP::PrepareInfo& info) override
            {
                static_cast<void>(info);
            }

            void process(SushiDSP::ProcessContext& context) override
            {
                SD_ZONE("test.counting");
                ++blocks;
                for (std::uint32_t i = 0; i < context.frames; ++i)
                    context.outputs[0][i] = context.inputs[0][i];
            }

            SushiDSP::PortLayout ports() const override
            {
                return {1, 1};
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

            static inline int blocks = 0;
    };
} // namespace

TEST(Medium_ProfiledNode, ProfileNodeProcessesOneBlockPerPeriodAndRecordsItsZone)
{
    CountingNode::blocks = 0;

    const SushiDSP::profile::ProfiledRun run = SushiDSP::bench::profile_node<CountingNode>(2, 4);

    if (!run.start.started())
        GTEST_SKIP() << "this build records nothing";
    EXPECT_EQ(CountingNode::blocks, 6);
    EXPECT_EQ(run.record.zone_calls("test.counting"), 4u);
}
