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
/* Licensed under the Apache License, Version 2.0 (the "License"); you    */
/* may not use this file except in compliance with the License. You may   */
/* obtain a copy of the License at                                        */
/*                                                                        */
/*     https://www.apache.org/licenses/LICENSE-2.0                        */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

/**
 * @file test_rt_graph_slot.cpp
 * @brief Tests the publish, acquire and reclaim sequence of RtGraphSlot.
 * @author Mustafa Garip
 */

#include <atomic>
#include <functional>
#include <memory>
#include <thread>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/core/audio_node.hpp>
#include <SushiDSP/foundation/core/rig_graph.hpp>
#include <SushiDSP/foundation/core/rt_graph_slot.hpp>

#include "support/in_memory_nodes.hpp"

namespace
{
    /// @brief Returns a graph of one mono node whose virtual ports() returns a fixed, checkable value.
    /// @see tests/foundation/core/README.md
    std::unique_ptr<SushiDSP::RigGraph> make_canary_graph()
    {
        auto graph = std::make_unique<SushiDSP::RigGraph>(256, 4);
        graph->add_node(TestSupport::gain_node(1.0f));
        return graph;
    }

    /// @brief Returns a graph of one mono node that counts its destruction in @p record.
    std::unique_ptr<SushiDSP::RigGraph> make_counted_graph(SushiDSP::InMemoryNodeRecord& record)
    {
        auto graph = std::make_unique<SushiDSP::RigGraph>(256, 4);
        graph->add_node(TestSupport::recording_sink_node(record));
        return graph;
    }

    /// @brief Returns whether @p graph still holds its one mono node, read through a virtual call.
    bool canary_is_intact(SushiDSP::RigGraph& graph)
    {
        const auto handles = graph.nodes();
        if (handles.size() != 1u)
        {
            return false;
        }
        const SushiDSP::AudioNode* node = graph.node(handles[0]);
        if (node == nullptr)
        {
            return false;
        }
        const SushiDSP::PortLayout layout = node->ports();
        return layout.input_channels == 1u && layout.output_channels == 1u;
    }

    /// @brief Acquires from @p slot until @p stop is set and stores whether every graph it saw was intact.
    void acquire_until_stopped(SushiDSP::RtGraphSlot& slot, const std::atomic<bool>& stop, std::atomic<bool>& intact)
    {
        while (!stop.load(std::memory_order_relaxed))
        {
            SushiDSP::RigGraph* graph = slot.acquire_for_audio_thread();
            if (graph == nullptr || !canary_is_intact(*graph))
            {
                intact.store(false, std::memory_order_relaxed);
            }
        }
    }

    /// @brief Publishes @p count canary graphs into @p slot, reclaiming after each one.
    void publish_and_reclaim(SushiDSP::RtGraphSlot& slot, int count)
    {
        for (int index = 0; index < count; ++index)
        {
            slot.publish(make_canary_graph());
            slot.reclaim();
        }
    }
} // namespace

TEST(Small_RtGraphSlot, RtGraphSlotStartsWithNoActiveGraph)
{
    SushiDSP::RtGraphSlot slot;
    EXPECT_EQ(slot.acquire_for_audio_thread(), nullptr);
}

TEST(Small_RtGraphSlot, RtGraphSlotPublishMakesGraphActive)
{
    SushiDSP::RtGraphSlot slot;
    auto graph = std::make_unique<SushiDSP::RigGraph>(256, 4);
    SushiDSP::RigGraph* raw = graph.get();

    slot.publish(std::move(graph));

    EXPECT_EQ(slot.acquire_for_audio_thread(), raw);
}

TEST(Small_RtGraphSlot, RtGraphSlotReclaimFreesOldGraphAfterAudioThreadMovesOn)
{
    SushiDSP::InMemoryNodeRecord first_record;
    SushiDSP::RtGraphSlot slot;
    slot.publish(make_counted_graph(first_record));
    slot.acquire_for_audio_thread();

    slot.publish(std::make_unique<SushiDSP::RigGraph>(256, 4));
    slot.acquire_for_audio_thread();
    slot.reclaim();

    EXPECT_EQ(first_record.destroyed, 1u);
}

TEST(Small_RtGraphSlot, RtGraphSlotReclaimKeepsAGraphTheAudioThreadIsStillOn)
{
    SushiDSP::InMemoryNodeRecord first_record;
    SushiDSP::RtGraphSlot slot;
    slot.publish(make_counted_graph(first_record));
    slot.acquire_for_audio_thread();

    slot.publish(std::make_unique<SushiDSP::RigGraph>(256, 4));
    slot.reclaim();

    EXPECT_EQ(first_record.destroyed, 0u);
}

TEST(Medium_RtGraphSlot, RtGraphSlotConcurrentPublishAndAcquireDoesNotRace)
{
    constexpr int K_PUBLISHES = 20000;
    SushiDSP::RtGraphSlot slot;
    slot.publish(make_canary_graph());
    std::atomic<bool> stop{false};
    std::atomic<bool> intact{true};

    // A virtual call catches a dangling graph pointer. See tests/foundation/core/README.md.
    std::thread audio_thread(acquire_until_stopped, std::ref(slot), std::cref(stop), std::ref(intact));
    publish_and_reclaim(slot, K_PUBLISHES);
    stop.store(true, std::memory_order_relaxed);
    audio_thread.join();
    slot.reclaim();

    EXPECT_TRUE(intact.load(std::memory_order_relaxed));
}
