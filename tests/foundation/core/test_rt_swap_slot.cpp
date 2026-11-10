/**************************************************************************/
/* test_rt_swap_slot.cpp                                                  */
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
 * @file test_rt_swap_slot.cpp
 * @brief Tests the publish, acquire and reclaim sequence of RtSwapSlot.
 * @author Mustafa Garip
 */

#include <atomic>
#include <functional>
#include <memory>
#include <thread>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/allocation_trap/allocation_trap.hpp>
#include <SushiDSP/foundation/core/rt_swap_slot.hpp>

namespace
{
    /// @brief Declares a payload whose destructor is observable through a live count the test owns.
    /// @see tests/foundation/core/README.md
    class Counted
    {
        public:
            Counted(int value, std::atomic<int>& live_count) : value_(value), live_count_(live_count)
            {
                live_count_.fetch_add(1, std::memory_order_relaxed);
            }
            ~Counted()
            {
                live_count_.fetch_sub(1, std::memory_order_relaxed);
            }

            Counted(const Counted&) = delete;
            Counted& operator=(const Counted&) = delete;

            int value() const
            {
                return value_;
            }

        private:
            int value_;
            std::atomic<int>& live_count_;
    };

    /// @brief Acquires from @p slot until @p stop is set and stores whether every object read as 7.
    void acquire_until_stopped(SushiDSP::RtSwapSlot<Counted>& slot, const std::atomic<bool>& stop,
                               std::atomic<bool>& intact)
    {
        while (!stop.load(std::memory_order_relaxed))
        {
            const Counted* object = slot.acquire_for_audio_thread();
            if (object == nullptr || object->value() != 7)
            {
                intact.store(false, std::memory_order_relaxed);
            }
        }
    }

    /// @brief Publishes @p count objects of value 7 into @p slot, reclaiming after each one.
    void publish_and_reclaim(SushiDSP::RtSwapSlot<Counted>& slot, std::atomic<int>& live_count, int count)
    {
        for (int index = 0; index < count; ++index)
        {
            slot.publish(std::make_unique<Counted>(7, live_count));
            slot.reclaim();
        }
    }

    /// @brief Acquires from @p slot @p count times and returns how many acquires saw an active object.
    int acquire_many(SushiDSP::RtSwapSlot<int>& slot, int count)
    {
        int seen = 0;
        for (int index = 0; index < count; ++index)
        {
            const int* object = slot.acquire_for_audio_thread();
            seen += (object != nullptr && slot.has_active()) ? 1 : 0;
        }
        return seen;
    }
} // namespace

TEST(Small_RtSwapSlot, RtSwapSlotStartsWithNoActiveObject)
{
    SushiDSP::RtSwapSlot<int> slot;
    EXPECT_EQ(slot.acquire_for_audio_thread(), nullptr);
    EXPECT_FALSE(slot.has_active());
}

TEST(Small_RtSwapSlot, RtSwapSlotPublishMakesObjectActive)
{
    SushiDSP::RtSwapSlot<int> slot;
    auto object = std::make_unique<int>(42);
    int* raw = object.get();

    slot.publish(std::move(object));

    EXPECT_TRUE(slot.has_active());
    EXPECT_EQ(slot.acquire_for_audio_thread(), raw);
    EXPECT_EQ(*slot.acquire_for_audio_thread(), 42);
}

TEST(Small_RtSwapSlot, RtSwapSlotReclaimKeepsAnObjectTheAudioThreadIsStillOn)
{
    std::atomic<int> live_count{0};
    SushiDSP::RtSwapSlot<Counted> slot;
    slot.publish(std::make_unique<Counted>(1, live_count));
    slot.acquire_for_audio_thread();
    slot.publish(std::make_unique<Counted>(2, live_count));

    // Generation 1 stays alive while the thread is on it. See tests/foundation/core/README.md.
    slot.reclaim();

    EXPECT_EQ(live_count.load(std::memory_order_relaxed), 2);
}

TEST(Small_RtSwapSlot, RtSwapSlotReclaimDestroysARetiredObjectOnceAudioThreadHasMovedPast)
{
    std::atomic<int> live_count{0};
    SushiDSP::RtSwapSlot<Counted> slot;
    slot.publish(std::make_unique<Counted>(1, live_count));
    slot.acquire_for_audio_thread();
    slot.publish(std::make_unique<Counted>(2, live_count));
    slot.acquire_for_audio_thread();

    slot.reclaim();

    EXPECT_EQ(live_count.load(std::memory_order_relaxed), 1);
}

TEST(Small_RtSwapSlot, RtSwapSlotReclaimNeverDestroysTheActiveObject)
{
    std::atomic<int> live_count{0};
    SushiDSP::RtSwapSlot<Counted> slot;
    slot.publish(std::make_unique<Counted>(1, live_count));
    slot.acquire_for_audio_thread();
    slot.acquire_for_audio_thread();

    slot.reclaim();

    EXPECT_EQ(live_count.load(std::memory_order_relaxed), 1);
}

TEST(Small_RtSwapSlot, RtSwapSlotDestructionFreesWhatItStillOwns)
{
    std::atomic<int> live_count{0};
    {
        SushiDSP::RtSwapSlot<Counted> slot;
        slot.publish(std::make_unique<Counted>(1, live_count));
        slot.publish(std::make_unique<Counted>(2, live_count));
    }

    EXPECT_EQ(live_count.load(std::memory_order_relaxed), 0);
}

TEST(Medium_RtSwapSlot, RtSwapSlotConcurrentPublishAndAcquireDoesNotRace)
{
    constexpr int K_PUBLISHES = 20000;
    std::atomic<int> live_count{0};
    std::atomic<bool> intact{true};
    {
        SushiDSP::RtSwapSlot<Counted> slot;
        slot.publish(std::make_unique<Counted>(7, live_count));
        std::atomic<bool> stop{false};

        std::thread audio_thread(acquire_until_stopped, std::ref(slot), std::cref(stop), std::ref(intact));
        publish_and_reclaim(slot, live_count, K_PUBLISHES);
        stop.store(true, std::memory_order_relaxed);
        audio_thread.join();
        slot.reclaim();
    }

    EXPECT_TRUE(intact.load(std::memory_order_relaxed));
    EXPECT_EQ(live_count.load(std::memory_order_relaxed), 0);
}

TEST(Small_RtSwapSlot, RtSwapSlotActiveForPublisherReturnsTheActiveObject)
{
    std::atomic<int> live_count{0};
    SushiDSP::RtSwapSlot<Counted> slot;
    slot.publish(std::make_unique<Counted>(1, live_count));
    slot.publish(std::make_unique<Counted>(2, live_count));

    EXPECT_EQ(slot.active_for_publisher()->value(), 2);
}

TEST(Small_RtSwapSlot, RtSwapSlotActiveForPublisherDoesNotAdvanceTheAudioThreadWatermark)
{
    std::atomic<int> live_count{0};
    SushiDSP::RtSwapSlot<Counted> slot;
    slot.publish(std::make_unique<Counted>(1, live_count));
    slot.acquire_for_audio_thread();
    slot.publish(std::make_unique<Counted>(2, live_count));

    // Reading the active object must not move the watermark. See tests/foundation/core/README.md.
    static_cast<void>(slot.active_for_publisher());
    slot.reclaim();

    EXPECT_EQ(live_count.load(std::memory_order_relaxed), 2);
}

TEST(Small_RtSwapSlot, RtSwapSlotOnlyAcquireForAudioThreadAdvancesTheWatermark)
{
    std::atomic<int> live_count{0};
    SushiDSP::RtSwapSlot<Counted> slot;
    slot.publish(std::make_unique<Counted>(1, live_count));
    slot.acquire_for_audio_thread();
    slot.publish(std::make_unique<Counted>(2, live_count));
    static_cast<void>(slot.active_for_publisher());

    // Only acquire_for_audio_thread() moves the watermark. See tests/foundation/core/README.md.
    slot.acquire_for_audio_thread();
    slot.reclaim();

    EXPECT_EQ(live_count.load(std::memory_order_relaxed), 1);
}

TEST(Medium_RtSwapSlot, AcquireForAudioThreadPerformsNoHeapAllocation)
{
    constexpr int K_ACQUIRES = 10000;
    SushiDSP::RtSwapSlot<int> slot;
    slot.publish(std::make_unique<int>(3));

    SushiDSP::test::AllocationTrap trap;
    trap.arm();
    const int seen = acquire_many(slot, K_ACQUIRES);
    trap.disarm();

    EXPECT_EQ(seen, K_ACQUIRES);
    EXPECT_EQ(trap.count(), 0);
}
