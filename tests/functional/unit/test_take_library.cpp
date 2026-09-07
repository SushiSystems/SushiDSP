/**************************************************************************/
/* test_take_library.cpp                                                  */
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

#include <cstddef>
#include <memory>

#include <gtest/gtest.h>

#include "SushiDSP/capture/Take.hpp"
#include "SushiDSP/capture/TakeLibrary.hpp"
#include "SushiDSP/core/RtSwapSlot.hpp"

namespace
{
    /// @brief Stands in for the TrackTimeline of spec §5: it retains a borrowed take for as long
    /// as it lives and releases it in its destructor, so a lane slot's reclaim() is what ends the
    /// hold. Owning the library by reference keeps the retain and the release paired.
    class HoldingTimeline
    {
    public:
        /// @brief Records one more holder of the take.
        /// @param library The library that owns the take.
        /// @param take The borrowed take this timeline names.
        HoldingTimeline(SushiDSP::TakeLibrary& library, const SushiDSP::Take* take)
            : library_(library), take_(take)
        {
            library_.retain(take_);
        }

        /// @brief Records one fewer holder of the take.
        ~HoldingTimeline()
        {
            library_.release(take_);
        }

        HoldingTimeline(const HoldingTimeline&) = delete;
        HoldingTimeline& operator=(const HoldingTimeline&) = delete;

        /// @brief Returns the take this timeline names.
        const SushiDSP::Take* take() const noexcept { return take_; }

    private:
        /// @brief Refers to the library that owns the take.
        SushiDSP::TakeLibrary& library_;

        /// @brief Points at the borrowed take, owned by library_.
        const SushiDSP::Take*  take_;
    };
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
    SushiDSP::RtSwapSlot<const HoldingTimeline>  lane;

    const SushiDSP::Take* take =
        library.insert(std::make_unique<const SushiDSP::Take>(1u, 48000.0, 128u));
    lane.publish(std::make_unique<const HoldingTimeline>(library, take));

    const HoldingTimeline* acquired = lane.acquire_for_audio_thread();
    ASSERT_NE(acquired, nullptr);
    EXPECT_EQ(acquired->take(), take);

    lane.publish(std::make_unique<const HoldingTimeline>(library, take));
    lane.reclaim();
    library.collect();

    // The first timeline is retired but the audio thread's watermark has not passed it, so both
    // timelines still name the take and the take must survive.
    EXPECT_EQ(library.size(), 1u);
    EXPECT_EQ(library.reference_count(take), 2u);
    EXPECT_EQ(lane.active_for_publisher()->take()->channel(0), take->channel(0));
}

TEST(Unit_Dsp, TakeLibraryFreesATakeOnlyAfterEveryLaneHasBeenReclaimed)
{
    SushiDSP::TakeLibrary                       library;
    SushiDSP::RtSwapSlot<const HoldingTimeline> lane;

    const SushiDSP::Take* take =
        library.insert(std::make_unique<const SushiDSP::Take>(1u, 48000.0, 128u));
    lane.publish(std::make_unique<const HoldingTimeline>(library, take));
    lane.acquire_for_audio_thread();
    lane.publish(std::make_unique<const HoldingTimeline>(library, take));
    lane.acquire_for_audio_thread();

    lane.reclaim();
    library.collect();
    EXPECT_EQ(library.size(), 1u);
    EXPECT_EQ(library.reference_count(take), 1u);
}
