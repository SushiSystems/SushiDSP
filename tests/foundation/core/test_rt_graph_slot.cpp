/**************************************************************************/
/* test_rt_graph_slot.cpp                                                 */
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
 * @file test_rt_graph_slot.cpp
 * @brief Tests the publish, acquire and reclaim sequence of RtGraphSlot.
 * @author Mustafa Garip
 */

#include <atomic>
#include <memory>
#include <thread>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/core/audio_node.hpp>
#include <SushiDSP/foundation/core/rig_graph.hpp>
#include <SushiDSP/foundation/core/rt_graph_slot.hpp>

namespace
{
    /// @brief Declares a node whose virtual ports() returns a fixed, checkable value.
    /// @see tests/foundation/core/README.md
    class CanaryNode : public SushiDSP::AudioNode
    {
        public:
            void prepare(const SushiDSP::PrepareInfo&) override {}
            void process(SushiDSP::ProcessContext&) override {}
            SushiDSP::PortLayout ports() const override
            {
                return {1, 1};
            }
            void reset() override {}
            float get_parameter(const char*) const override
            {
                return 0.0f;
            }
            void set_parameter_for_control_thread(const char*, float) override {}
    };

    std::unique_ptr<SushiDSP::RigGraph> make_canary_graph()
    {
        auto graph = std::make_unique<SushiDSP::RigGraph>(256, 4);
        graph->add_node(std::make_unique<CanaryNode>());
        return graph;
    }
} // namespace

TEST(Unit_Dsp, RtGraphSlotStartsWithNoActiveGraph)
{
    SushiDSP::RtGraphSlot slot;
    EXPECT_EQ(slot.acquire_for_audio_thread(), nullptr);
}

TEST(Unit_Dsp, RtGraphSlotPublishMakesGraphActive)
{
    SushiDSP::RtGraphSlot slot;
    auto graph = std::make_unique<SushiDSP::RigGraph>(256, 4);
    SushiDSP::RigGraph* raw = graph.get();

    slot.publish(std::move(graph));

    EXPECT_EQ(slot.acquire_for_audio_thread(), raw);
}

TEST(Unit_Dsp, RtGraphSlotReclaimFreesOldGraphAfterAudioThreadMovesOn)
{
    SushiDSP::RtGraphSlot slot;
    slot.publish(std::make_unique<SushiDSP::RigGraph>(256, 4));
    slot.acquire_for_audio_thread(); // audio thread "sees" generation 1

    auto second = std::make_unique<SushiDSP::RigGraph>(256, 4);
    SushiDSP::RigGraph* second_raw = second.get();
    slot.publish(std::move(second));

    slot.acquire_for_audio_thread(); // audio thread now "sees" generation 2
    slot.reclaim();                  // safe to free generation 1's graph now

    EXPECT_EQ(slot.acquire_for_audio_thread(), second_raw);
}

TEST(Unit_Dsp, RtGraphSlotConcurrentPublishAndAcquireDoesNotRace)
{
    SushiDSP::RtGraphSlot slot;
    slot.publish(make_canary_graph());

    std::atomic<bool> stop{false};

    std::thread audio_thread(
        [&]
        {
            while (!stop.load(std::memory_order_relaxed))
            {
                SushiDSP::RigGraph* graph = slot.acquire_for_audio_thread();
                ASSERT_NE(graph, nullptr);

                // A virtual call catches a dangling graph pointer. See tests/foundation/core/README.md.
                auto handles = graph->nodes();
                ASSERT_EQ(handles.size(), 1u);
                SushiDSP::AudioNode* node = graph->node(handles[0]);
                ASSERT_NE(node, nullptr);
                const SushiDSP::PortLayout layout = node->ports();
                EXPECT_EQ(layout.input_channels, 1u);
                EXPECT_EQ(layout.output_channels, 1u);
            }
        });

    for (int i = 0; i < 20000; ++i)
    {
        slot.publish(make_canary_graph());
        slot.reclaim();
    }

    stop.store(true, std::memory_order_relaxed);
    audio_thread.join();
    slot.reclaim();
}
