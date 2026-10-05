/****************************************************************/
/* test_take_library.cpp                                        */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file test_take_library.cpp
 * @brief Tests the lifetime of takes held in TakeLibrary.
 * @author Mustafa Garip
 */

#include <cstddef>
#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "SushiDSP/capture/Take.hpp"
#include "SushiDSP/capture/TakeLibrary.hpp"
#include "SushiDSP/capture/TrackTimeline.hpp"
#include "SushiDSP/core/RtSwapSlot.hpp"

namespace
{
    /// @brief Builds a one-clip timeline naming a take, which retains it for as long as the
    /// timeline lives and releases it in its destructor, so a lane slot's reclaim() is what
    /// ends the hold.
    std::unique_ptr<const SushiDSP::TrackTimeline> holding_timeline(
        SushiDSP::TakeLibrary& library, const SushiDSP::Take* take)
    {
        const std::vector<SushiDSP::Clip> clips{ SushiDSP::Clip{ take, 0u, 0u, 128u } };
        return std::make_unique<const SushiDSP::TrackTimeline>(library, clips);
    }
}

TEST(Unit_Dsp, TakeLibraryKeepsATakeAliveWhileSomethingHoldsIt)
{
    SushiDSP::TakeLibrary library;
    const SushiDSP::Take* take =
        library.insert(std::make_unique<const SushiDSP::Take>(1u, 48000.0, 128u));
    library.retain(take);
    library.collect();
    EXPECT_EQ(library.size(), 1u);
    EXPECT_EQ(library.reference_count(take), 1u);
}

TEST(Unit_Dsp, TakeLibraryFreesATakeOnlyWhenTheLastHolderReleasesIt)
{
    SushiDSP::TakeLibrary library;
    const SushiDSP::Take* take =
        library.insert(std::make_unique<const SushiDSP::Take>(1u, 48000.0, 128u));
    library.retain(take);
    library.retain(take);
    library.release(take);
    library.collect();
    EXPECT_EQ(library.size(), 1u);
    library.release(take);
    library.collect();
    EXPECT_EQ(library.size(), 0u);
}

TEST(Unit_Dsp, TakeLibraryDropsATakeNothingEverHeld)
{
    SushiDSP::TakeLibrary library;
    library.insert(std::make_unique<const SushiDSP::Take>(1u, 48000.0, 128u));
    library.collect();
    EXPECT_EQ(library.size(), 0u);
}

TEST(Unit_Dsp, TakeLibraryGivesTwoHoldersOneCopyOfTheSamples)
{
    SushiDSP::TakeLibrary library;
    const SushiDSP::Take* first =
        library.insert(std::make_unique<const SushiDSP::Take>(2u, 48000.0, 4096u));
    library.retain(first);
    library.retain(first);
    const SushiDSP::Take* second = first;
    EXPECT_EQ(first->channel(0), second->channel(0));
    EXPECT_EQ(library.size(), 1u);
}

TEST(Unit_Dsp, TakeLibraryHoldsATakeATimelineStillInTheRetireListNames)
{
    SushiDSP::TakeLibrary                        library;
    SushiDSP::RtSwapSlot<const SushiDSP::TrackTimeline>  lane;

    const SushiDSP::Take* take =
        library.insert(std::make_unique<const SushiDSP::Take>(1u, 48000.0, 128u));
    lane.publish(holding_timeline(library, take));

    const SushiDSP::TrackTimeline* acquired = lane.acquire_for_audio_thread();
    ASSERT_NE(acquired, nullptr);
    EXPECT_EQ(acquired->clip(0).take, take);

    lane.publish(holding_timeline(library, take));
    lane.reclaim();
    library.collect();

    // A retired timeline still names the take. See tests/functional/unit/README.md.
    EXPECT_EQ(library.size(), 1u);
    EXPECT_EQ(library.reference_count(take), 2u);
    EXPECT_EQ(lane.active_for_publisher()->clip(0).take->channel(0), take->channel(0));
}

TEST(Unit_Dsp, TakeLibraryFreesATakeOnlyAfterEveryLaneHasBeenReclaimed)
{
    SushiDSP::TakeLibrary                       library;
    SushiDSP::RtSwapSlot<const SushiDSP::TrackTimeline> lane;

    const SushiDSP::Take* take =
        library.insert(std::make_unique<const SushiDSP::Take>(1u, 48000.0, 128u));
    lane.publish(holding_timeline(library, take));
    lane.acquire_for_audio_thread();
    lane.publish(holding_timeline(library, take));
    lane.acquire_for_audio_thread();

    lane.reclaim();
    library.collect();
    EXPECT_EQ(library.size(), 1u);
    EXPECT_EQ(library.reference_count(take), 1u);
}
