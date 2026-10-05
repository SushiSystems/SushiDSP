/****************************************************************/
/* test_rt_swap_slot.cpp                                        */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

#include <atomic>
#include <memory>
#include <thread>

#include <gtest/gtest.h>

#include "SushiDSP/core/RtSwapSlot.hpp"

namespace
{
    // A payload whose destructor is observable, so the deferred-reclaim contract can be checked
    // by counting frees rather than by inspecting RtSwapSlot's internals. The canary value is
    // dereferenced (not just null-checked) by the concurrent case: a dangling-but-non-null
    // pointer sails straight through a null-check, whereas reading a field of freed memory that
    // has since been reused shows up as a wrong value or a crash.
    class Counted
    {
    public:
        static std::atomic<int> live_count;

        explicit Counted(int value) : value_(value) { live_count.fetch_add(1, std::memory_order_relaxed); }
        ~Counted() { live_count.fetch_sub(1, std::memory_order_relaxed); }

        Counted(const Counted&) = delete;
        Counted& operator=(const Counted&) = delete;

        int value() const { return value_; }

    private:
        int value_;
    };

    std::atomic<int> Counted::live_count{ 0 };
}

TEST(Unit_Dsp, RtSwapSlotStartsWithNoActiveObject)
{
    SushiDSP::RtSwapSlot<int> slot;
    EXPECT_EQ(slot.acquire_for_audio_thread(), nullptr);
    EXPECT_FALSE(slot.has_active());
}

TEST(Unit_Dsp, RtSwapSlotPublishMakesObjectActive)
{
    SushiDSP::RtSwapSlot<int> slot;
    auto object = std::make_unique<int>(42);
    int* raw = object.get();

    slot.publish(std::move(object));

    EXPECT_TRUE(slot.has_active());
    EXPECT_EQ(slot.acquire_for_audio_thread(), raw);
    EXPECT_EQ(*slot.acquire_for_audio_thread(), 42);
}

TEST(Unit_Dsp, RtSwapSlotReclaimFreesOldObjectAfterAudioThreadMovesOn)
{
    SushiDSP::RtSwapSlot<int> slot;
    slot.publish(std::make_unique<int>(1));
    slot.acquire_for_audio_thread();  // audio thread "sees" generation 1

    auto second = std::make_unique<int>(2);
    int* second_raw = second.get();
    slot.publish(std::move(second));

    slot.acquire_for_audio_thread();  // audio thread now "sees" generation 2
    slot.reclaim();                   // safe to free generation 1's object now

    EXPECT_EQ(slot.acquire_for_audio_thread(), second_raw);
}

TEST(Unit_Dsp, RtSwapSlotReclaimDestroysRetiredObjectsOnlyOnceAudioThreadHasMovedPast)
{
    Counted::live_count.store(0, std::memory_order_relaxed);

    {
        SushiDSP::RtSwapSlot<Counted> slot;
        slot.publish(std::make_unique<Counted>(1));
        slot.acquire_for_audio_thread();
        EXPECT_EQ(Counted::live_count.load(std::memory_order_relaxed), 1);

        slot.publish(std::make_unique<Counted>(2));

        // The audio thread is still parked on generation 1, so generation 1's object must stay
        // alive no matter how often reclaim() runs.
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

TEST(Unit_Dsp, RtSwapSlotConcurrentPublishAndAcquireDoesNotRace)
{
    Counted::live_count.store(0, std::memory_order_relaxed);

    {
        SushiDSP::RtSwapSlot<Counted> slot;
        slot.publish(std::make_unique<Counted>(7));

        std::atomic<bool> stop{ false };

        std::thread audio_thread([&]
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

TEST(Unit_Dsp, RtSwapSlotActiveForPublisherDoesNotAdvanceTheAudioThreadWatermark)
{
    // active_for_publisher() exists so the publishing thread can look at the active object
    // without claiming the audio thread reached it. Reading it must therefore leave reclaim()'s
    // "provably moved past" bound exactly where it was - if it forged the watermark instead,
    // reclaim() would free the object an audio thread is still using.
    Counted::live_count.store(0, std::memory_order_relaxed);
    {
        SushiDSP::RtSwapSlot<Counted> slot;

        slot.publish(std::make_unique<Counted>(1));
        Counted* const first = slot.acquire_for_audio_thread();
        ASSERT_NE(first, nullptr);
        EXPECT_EQ(first->value(), 1);

        slot.publish(std::make_unique<Counted>(2));

        EXPECT_EQ(slot.active_for_publisher(), slot.acquire_for_audio_thread());

        // The audio thread's last recorded generation is still the second acquire above, so the
        // first object is genuinely retired here. Re-read it through the publisher accessor and
        // confirm that read alone did not move anything.
        EXPECT_EQ(slot.active_for_publisher()->value(), 2);

        slot.publish(std::make_unique<Counted>(3));
        EXPECT_EQ(slot.active_for_publisher()->value(), 3);

        // The audio thread has not acquired since generation 2 was published, so reclaim() may
        // free nothing published at or after it: object 2 (still the generation the audio thread
        // last recorded) and object 3 (active) must both survive. Only object 1 may go.
        slot.reclaim();
        EXPECT_EQ(Counted::live_count.load(std::memory_order_relaxed), 2);

        // Once the audio thread does acquire, the earlier object becomes reclaimable - proving
        // the watermark was still being driven by acquire_for_audio_thread() alone.
        ASSERT_EQ(slot.acquire_for_audio_thread()->value(), 3);
        slot.reclaim();
        EXPECT_EQ(Counted::live_count.load(std::memory_order_relaxed), 1);
    }
    EXPECT_EQ(Counted::live_count.load(std::memory_order_relaxed), 0);
}
