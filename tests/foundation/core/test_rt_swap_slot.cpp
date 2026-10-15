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
 * @file test_rt_swap_slot.cpp
 * @brief Tests the publish, acquire and reclaim sequence of RtSwapSlot.
 * @author Mustafa Garip
 */

#include <atomic>
#include <memory>
#include <thread>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/allocation_trap/allocation_trap.hpp>
#include <SushiDSP/foundation/core/rt_swap_slot.hpp>

namespace
{
    /// @brief Declares a payload whose destructor is observable through a live count.
    /// @see tests/foundation/core/README.md
    class Counted
    {
        public:
            static std::atomic<int> live_count;

            explicit Counted(int value) : value_(value)
            {
                live_count.fetch_add(1, std::memory_order_relaxed);
            }
            ~Counted()
            {
                live_count.fetch_sub(1, std::memory_order_relaxed);
            }

            Counted(const Counted&) = delete;
            Counted& operator=(const Counted&) = delete;

            int value() const
            {
                return value_;
            }

        private:
            int value_;
    };

    std::atomic<int> Counted::live_count{0};
} // namespace

TEST(RtSwapSlot, RtSwapSlotStartsWithNoActiveObject)
{
    SushiDSP::RtSwapSlot<int> slot;
    EXPECT_EQ(slot.acquire_for_audio_thread(), nullptr);
    EXPECT_FALSE(slot.has_active());
}

TEST(RtSwapSlot, RtSwapSlotPublishMakesObjectActive)
{
    SushiDSP::RtSwapSlot<int> slot;
    auto object = std::make_unique<int>(42);
    int* raw = object.get();

    slot.publish(std::move(object));

    EXPECT_TRUE(slot.has_active());
    EXPECT_EQ(slot.acquire_for_audio_thread(), raw);
    EXPECT_EQ(*slot.acquire_for_audio_thread(), 42);
}

TEST(RtSwapSlot, RtSwapSlotReclaimFreesOldObjectAfterAudioThreadMovesOn)
{
    SushiDSP::RtSwapSlot<int> slot;
    slot.publish(std::make_unique<int>(1));
    slot.acquire_for_audio_thread(); // audio thread "sees" generation 1

    auto second = std::make_unique<int>(2);
    int* second_raw = second.get();
    slot.publish(std::move(second));

    slot.acquire_for_audio_thread(); // audio thread now "sees" generation 2
    slot.reclaim();                  // safe to free generation 1's object now

    EXPECT_EQ(slot.acquire_for_audio_thread(), second_raw);
}

TEST(RtSwapSlot, RtSwapSlotReclaimDestroysRetiredObjectsOnlyOnceAudioThreadHasMovedPast)
{
    Counted::live_count.store(0, std::memory_order_relaxed);

    {
        SushiDSP::RtSwapSlot<Counted> slot;
        slot.publish(std::make_unique<Counted>(1));
        slot.acquire_for_audio_thread();
        EXPECT_EQ(Counted::live_count.load(std::memory_order_relaxed), 1);

        slot.publish(std::make_unique<Counted>(2));

        // Generation 1 stays alive while the thread is on it. See tests/foundation/core/README.md.
        slot.reclaim();
        EXPECT_EQ(Counted::live_count.load(std::memory_order_relaxed), 2);

        slot.acquire_for_audio_thread();
        slot.reclaim();
        EXPECT_EQ(Counted::live_count.load(std::memory_order_relaxed), 1);

        // The active object is never reclaimed, however far the audio thread advances.
        slot.acquire_for_audio_thread();
        slot.reclaim();
        EXPECT_EQ(Counted::live_count.load(std::memory_order_relaxed), 1);
    }

    // Destroying the slot frees whatever it still owns; nothing may leak.
    EXPECT_EQ(Counted::live_count.load(std::memory_order_relaxed), 0);
}

TEST(RtSwapSlot, RtSwapSlotConcurrentPublishAndAcquireDoesNotRace)
{
    Counted::live_count.store(0, std::memory_order_relaxed);

    {
        SushiDSP::RtSwapSlot<Counted> slot;
        slot.publish(std::make_unique<Counted>(7));

        std::atomic<bool> stop{false};

        std::thread audio_thread(
            [&]
            {
                while (!stop.load(std::memory_order_relaxed))
                {
                    Counted* object = slot.acquire_for_audio_thread();
                    ASSERT_NE(object, nullptr);
                    EXPECT_EQ(object->value(), 7);
                }
            });

        for (int i = 0; i < 20000; ++i)
        {
            slot.publish(std::make_unique<Counted>(7));
            slot.reclaim();
        }

        stop.store(true, std::memory_order_relaxed);
        audio_thread.join();
        slot.reclaim();
    }

    EXPECT_EQ(Counted::live_count.load(std::memory_order_relaxed), 0);
}

TEST(RtSwapSlot, RtSwapSlotActiveForPublisherDoesNotAdvanceTheAudioThreadWatermark)
{
    // Reading the active object must not move the watermark. See tests/foundation/core/README.md.
    Counted::live_count.store(0, std::memory_order_relaxed);
    {
        SushiDSP::RtSwapSlot<Counted> slot;

        slot.publish(std::make_unique<Counted>(1));
        Counted* const first = slot.acquire_for_audio_thread();
        ASSERT_NE(first, nullptr);
        EXPECT_EQ(first->value(), 1);

        slot.publish(std::make_unique<Counted>(2));

        EXPECT_EQ(slot.active_for_publisher(), slot.acquire_for_audio_thread());

        // The read alone moves nothing. See tests/foundation/core/README.md.
        EXPECT_EQ(slot.active_for_publisher()->value(), 2);

        slot.publish(std::make_unique<Counted>(3));
        EXPECT_EQ(slot.active_for_publisher()->value(), 3);

        // Only object 1 may be freed here. See tests/foundation/core/README.md.
        slot.reclaim();
        EXPECT_EQ(Counted::live_count.load(std::memory_order_relaxed), 2);

        // Only acquire_for_audio_thread() moves the watermark. See tests/foundation/core/README.md.
        ASSERT_EQ(slot.acquire_for_audio_thread()->value(), 3);
        slot.reclaim();
        EXPECT_EQ(Counted::live_count.load(std::memory_order_relaxed), 1);
    }
    EXPECT_EQ(Counted::live_count.load(std::memory_order_relaxed), 0);
}

TEST(RtSwapSlot, AcquireForAudioThreadPerformsNoHeapAllocation)
{
    SushiDSP::RtSwapSlot<int> slot;
    slot.publish(std::make_unique<int>(3));

    SushiDSP::test::AllocationTrap trap;
    trap.arm();

    int accumulator = 0;
    for (int i = 0; i < 10000; ++i)
    {
        const int* object = slot.acquire_for_audio_thread();
        accumulator += (object != nullptr && slot.has_active()) ? *object : 0;
    }

    trap.disarm();

    EXPECT_EQ(accumulator, 30000);
    EXPECT_EQ(trap.count(), 0);
}
