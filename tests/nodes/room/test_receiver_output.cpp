/**************************************************************************/
/* test_receiver_output.cpp                                               */
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
 * @file test_receiver_output.cpp
 * @brief Tests the head holder's HRTF set and decoder, and how the ears and microphone outputs sum and align.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/nodes/room/hrtf_set.hpp>
#include <SushiDSP/nodes/room/receiver_output.hpp>
#include <SushiDSP/nodes/room/room_host.hpp>
#include <SushiDSP/nodes/room/spherical_head_hrtf.hpp>

#include <array>
#include <cstdint>
#include <stdexcept>

namespace
{
    using SushiDSP::Room::EarsOutput;
    using SushiDSP::Room::HeadHolder;
    using SushiDSP::Room::MicrophoneOutput;
    using SushiDSP::Room::PairStage;
    using SushiDSP::room::ListenerKind;

    constexpr double K_RATE = 48000.0;
    constexpr std::uint32_t K_BLOCK = 4;

    /// @brief Counts the frames the outputs hold the summed block back by in the alignment tests.
    constexpr std::uint32_t K_ALIGNMENT = 2;

    using Block = std::array<float, K_BLOCK>;

    const Block kFirst = {1.0f, 2.0f, 3.0f, 4.0f};
    const Block kSecond = {10.0f, 20.0f, 30.0f, 40.0f};
    const Block kFirstPlusSecond = {11.0f, 22.0f, 33.0f, 44.0f};
    const Block kSilence = {};

    /// @brief Returns a pair of the given kind, prepared with no set adopted, so every block it
    /// hands on is silent.
    void prepare_silent_pair(PairStage& pair, ListenerKind kind)
    {
        pair.prepare(kind, K_RATE, K_ALIGNMENT, K_BLOCK);
    }

    /// @brief Returns an ears output prepared at the given alignment.
    void prepare_ears(EarsOutput& output, std::uint32_t alignment)
    {
        output.prepare(alignment, K_BLOCK);
    }

    /// @brief Runs one block of the ears output over cabinets whose modal fields are the given
    /// blocks, with no decoder, and returns the left ear.
    Block run_ears(EarsOutput& output, const PairStage& pair, std::initializer_list<const Block*> modal_fields)
    {
        Block left{};
        Block right{};
        output.begin();
        for (const Block* modal : modal_fields)
        {
            output.add(pair, modal->data(), modal->data(), K_BLOCK);
        }
        output.finish(nullptr, left.data(), right.data(), K_BLOCK);
        return left;
    }

    /// @brief Runs one block of the microphone output over cabinets whose modal fields are the
    /// given blocks, and returns it.
    Block run_microphone(MicrophoneOutput& output, const PairStage& pair, std::initializer_list<const Block*> modal_fields)
    {
        Block block{};
        output.begin();
        for (const Block* modal : modal_fields)
        {
            output.add(pair, modal->data(), K_BLOCK);
        }
        output.finish(block.data(), K_BLOCK);
        return block;
    }
} // namespace

TEST(Small_ReceiverOutput, HeadHolderFallsBackToTheSphericalHeadWhenTheMeasuredSetIsAtAnotherRate)
{
    HeadHolder holder;
    holder.set_source(SushiDSP::room::HrtfSource::Measured);

    holder.refresh_head(48000.0);

    EXPECT_EQ(holder.source(), SushiDSP::room::HrtfSource::SphericalHead);
}

TEST(Small_ReceiverOutput, HeadHolderBuildsTheSphericalHeadAtTheRateItIsGiven)
{
    HeadHolder holder;
    holder.set_source(SushiDSP::room::HrtfSource::SphericalHead);

    holder.refresh_head(48000.0);

    EXPECT_EQ(holder.hrtf().sample_rate(), 48000.0);
}

TEST(Small_ReceiverOutput, HeadHolderTakeHrtfMovesTheSetOut)
{
    HeadHolder holder;
    holder.set_hrtf(SushiDSP::room::SphericalHeadHrtf::build(K_RATE));
    const std::uint32_t held = holder.hrtf().direction_count();

    const SushiDSP::room::HrtfSet taken = holder.take_hrtf();

    EXPECT_EQ(taken.direction_count(), held);
}

TEST(Small_ReceiverOutput, HeadHolderTakeHrtfLeavesAnEmptySetBehind)
{
    HeadHolder holder;
    holder.set_hrtf(SushiDSP::room::SphericalHeadHrtf::build(K_RATE));

    const SushiDSP::room::HrtfSet taken = holder.take_hrtf();

    EXPECT_TRUE(holder.hrtf().empty());
}

TEST(Small_ReceiverOutput, HeadHolderPublishDecoderThrowsWhenTheSetCarriesNoThirdOrderDecoder)
{
    HeadHolder holder;
    holder.set_hrtf(SushiDSP::room::HrtfSet());

    EXPECT_THROW(holder.publish_decoder(K_RATE, K_BLOCK), std::runtime_error);
}

TEST(Small_ReceiverOutput, HeadHolderAcquireDecoderReturnsNullBeforeTheFirstPublish)
{
    HeadHolder holder;

    EXPECT_EQ(holder.acquire_decoder(), nullptr);
}

TEST(Small_ReceiverOutput, EarsOutputCopiesTheFirstPairOfABlock)
{
    PairStage pair;
    prepare_silent_pair(pair, ListenerKind::Ears);
    EarsOutput output;
    prepare_ears(output, 0);

    EXPECT_EQ(run_ears(output, pair, {&kFirst}), kFirst);
}

TEST(Small_ReceiverOutput, EarsOutputAddsTheRestOfTheBlockToTheFirstPair)
{
    PairStage pair;
    prepare_silent_pair(pair, ListenerKind::Ears);
    EarsOutput output;
    prepare_ears(output, 0);

    EXPECT_EQ(run_ears(output, pair, {&kFirst, &kSecond}), kFirstPlusSecond);
}

TEST(Small_ReceiverOutput, EarsOutputForgetsTheLastBlockWhenANewOneBegins)
{
    PairStage pair;
    prepare_silent_pair(pair, ListenerKind::Ears);
    EarsOutput output;
    prepare_ears(output, 0);
    run_ears(output, pair, {&kFirst});

    EXPECT_EQ(run_ears(output, pair, {&kSecond}), kSecond);
}

TEST(Small_ReceiverOutput, EarsOutputHoldsTheModalFieldBackByTheAlignment)
{
    PairStage pair;
    prepare_silent_pair(pair, ListenerKind::Ears);
    EarsOutput output;
    prepare_ears(output, K_ALIGNMENT);

    EXPECT_EQ(run_ears(output, pair, {&kFirst}), (Block{0.0f, 0.0f, 1.0f, 2.0f}));
}

TEST(Small_ReceiverOutput, EarsOutputIsSilentWhenNoPairWasAdded)
{
    PairStage pair;
    prepare_silent_pair(pair, ListenerKind::Ears);
    EarsOutput output;
    prepare_ears(output, 0);
    run_ears(output, pair, {&kFirst});

    EXPECT_EQ(run_ears(output, pair, {}), kSilence);
}

TEST(Small_ReceiverOutput, MicrophoneOutputCopiesTheFirstCabinetOfABlock)
{
    PairStage pair;
    prepare_silent_pair(pair, ListenerKind::Microphone);
    MicrophoneOutput output;
    output.prepare(0, K_BLOCK);

    EXPECT_EQ(run_microphone(output, pair, {&kFirst}), kFirst);
}

TEST(Small_ReceiverOutput, MicrophoneOutputSumsEachCabinetsModalField)
{
    PairStage pair;
    prepare_silent_pair(pair, ListenerKind::Microphone);
    MicrophoneOutput output;
    output.prepare(0, K_BLOCK);

    EXPECT_EQ(run_microphone(output, pair, {&kFirst, &kSecond}), kFirstPlusSecond);
}

TEST(Small_ReceiverOutput, MicrophoneOutputDelaysTheSummedBlockByTheAlignment)
{
    PairStage pair;
    prepare_silent_pair(pair, ListenerKind::Microphone);
    MicrophoneOutput output;
    output.prepare(K_ALIGNMENT, K_BLOCK);

    EXPECT_EQ(run_microphone(output, pair, {&kFirst}), (Block{0.0f, 0.0f, 1.0f, 2.0f}));
}
