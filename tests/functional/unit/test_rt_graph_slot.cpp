/**************************************************************************/
/* test_rt_graph_slot.cpp                                                 */
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

#include <atomic>
#include <memory>
#include <thread>

#include <gtest/gtest.h>

#include "SushiDSP/core/IAudioNode.hpp"
#include "SushiDSP/core/RigGraph.hpp"
#include "SushiDSP/core/RtGraphSlot.hpp"

namespace
{
    // A minimal node whose ports() is a virtual call returning a fixed, checkable value. Poking
    // it after the owning RigGraph has (incorrectly) been freed exercises the object's vtable
    // pointer -- freed memory that's since been reused for something else will very likely make
    // this crash outright (invalid/foreign vtable) rather than just quietly return stale data,
    // which is a much stronger use-after-free signal than a bare null-check on the RigGraph*.
    class CanaryNode : public SushiDSP::IAudioNode
    {
    public:
        void prepare(const SushiDSP::PrepareInfo&) override {}
        void process(SushiDSP::ProcessContext&) override {}
        SushiDSP::PortLayout ports() const override { return { 1, 1 }; }
        void reset() override {}
        float get_parameter(const char*) const override { return 0.0f; }
        void set_parameter_for_control_thread(const char*, float) override {}
    };

    std::unique_ptr<SushiDSP::RigGraph> make_canary_graph()
    {
        auto graph = std::make_unique<SushiDSP::RigGraph>(256, 4);
        graph->add_node(std::make_unique<CanaryNode>());
        return graph;
    }
}

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
    slot.acquire_for_audio_thread();  // audio thread "sees" generation 1

    auto second = std::make_unique<SushiDSP::RigGraph>(256, 4);
    SushiDSP::RigGraph* second_raw = second.get();
    slot.publish(std::move(second));

    slot.acquire_for_audio_thread();  // audio thread now "sees" generation 2
    slot.reclaim();                   // safe to free generation 1's graph now

    EXPECT_EQ(slot.acquire_for_audio_thread(), second_raw);
}

TEST(Unit_Dsp, RtGraphSlotConcurrentPublishAndAcquireDoesNotRace)
{
    SushiDSP::RtGraphSlot slot;
    slot.publish(make_canary_graph());

    std::atomic<bool> stop{ false };

    std::thread audio_thread([&]
    {
        while (!stop.load(std::memory_order_relaxed))
        {
            SushiDSP::RigGraph* graph = slot.acquire_for_audio_thread();
            ASSERT_NE(graph, nullptr);

            // Actually dereference the returned graph, through a virtual call, rather than just
            // null-checking the pointer -- a dangling-but-non-null RigGraph* sails straight
            // through a null-check. Every published graph here carries exactly one CanaryNode,
            // so nodes() must come back with exactly one handle, and that node's ports() (a
            // virtual call) must always report { 1, 1 }. If acquire_for_audio_thread() ever
            // hands back a pointer to a graph reclaim() already freed, this either crashes
            // outright (the vtable slot now belongs to whatever reused the memory) or the
            // checks below fail.
            auto handles = graph->nodes();
            ASSERT_EQ(handles.size(), 1u);
            SushiDSP::IAudioNode* node = graph->node(handles[0]);
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
