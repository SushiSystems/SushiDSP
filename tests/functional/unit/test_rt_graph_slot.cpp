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

#include "SushiDSP/core/RigGraph.hpp"
#include "SushiDSP/core/RtGraphSlot.hpp"

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
    slot.publish(std::make_unique<SushiDSP::RigGraph>(256, 4));

    std::atomic<bool> stop{ false };

    std::thread audio_thread([&]
    {
        while (!stop.load(std::memory_order_relaxed))
        {
            SushiDSP::RigGraph* graph = slot.acquire_for_audio_thread();
            EXPECT_NE(graph, nullptr);
        }
    });

    for (int i = 0; i < 200; ++i)
    {
        slot.publish(std::make_unique<SushiDSP::RigGraph>(256, 4));
        slot.reclaim();
        std::this_thread::yield();
    }

    stop.store(true, std::memory_order_relaxed);
    audio_thread.join();
    slot.reclaim();
}
