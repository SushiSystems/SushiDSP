/**************************************************************************/
/* test_early_reflection_renderer.cpp                                     */
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
 * @file test_early_reflection_renderer.cpp
 * @brief Tests for EarlyReflectionRenderer, driven by hand-written tap sets with no solver.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <SushiDSP/foundation/allocation_trap/allocation_trap.hpp>
#include <SushiDSP/nodes/room/delay_ring.hpp>
#include <SushiDSP/nodes/room/early_reflection_renderer.hpp>
#include <SushiDSP/nodes/room/early_tap_set.hpp>

#include <support/tolerance.hpp>

namespace
{
    using SushiDSP::room::DelayRing;
    using SushiDSP::room::EarlyReflectionRenderer;
    using SushiDSP::room::EarlyTap;
    using SushiDSP::room::EarlyTapSet;

    /// @brief Counts the channels the renderer accumulates into.
    constexpr std::uint32_t K_CHANNELS = EarlyReflectionRenderer::K_OUTPUT_CHANNELS;

    /// @brief Holds the block length every test renders in.
    constexpr std::uint32_t K_BLOCK = 256;

    /// @brief Holds the longest delay the tests ask the ring to hold.
    constexpr std::uint32_t K_MAX_DELAY = 8397;

    /// @brief Holds pi.
    constexpr double K_PI = 3.14159265358979323846;

    /// @brief Holds the frequency of the sine the interpolation test delays, in hertz.
    constexpr double K_SINE_HZ = 1000.0;

    /// @brief Holds the sample rate of the sine, in hertz.
    constexpr double K_SAMPLE_RATE = 48000.0;

    /// @brief Bounds the error of a cubic Lagrange read of a sine half a frame off the grid. The
    /// error of a cubic through four points is at most the fourth derivative over 4! times the
    /// product of the distances to the four nodes: w^4 of a sine of w rad per frame, and
    /// 1.5 * 0.5 * 0.5 * 1.5 = 0.5625 at a half-frame fraction. Float rounding of the input and the
    /// four weighted terms adds eight units of u.
    const double K_HALF_FRAME_BOUND =
        std::pow(2.0 * K_PI * K_SINE_HZ / K_SAMPLE_RATE, 4.0) * 0.5625 / 24.0 +
        8.0 * TestSupport::unit_roundoff<float>();

    /// @brief Bounds a weight the half-frame Lagrange kernel gives, 9/16: three roundings of
    /// the kernel's float terms.
    constexpr std::size_t K_KERNEL_OPERATIONS = 4;

    /// @brief Holds the pole radius of the resonator that rings for many blocks.
    constexpr double K_RING_RADIUS = 0.999;

    /// @brief Holds sixteen accumulation buffers and the pointer table the renderer takes.
    class OutputBlock
    {
        public:
            /// @brief Allocates zeroed buffers of one block each.
            OutputBlock()
            {
                for (std::uint32_t channel = 0; channel < K_CHANNELS; ++channel)
                {
                    buffers_[channel].assign(K_BLOCK, 0.0f);
                    pointers_[channel] = buffers_[channel].data();
                }
            }

            /// @brief Zeroes every channel.
            void clear()
            {
                for (auto& buffer : buffers_)
                {
                    buffer.assign(K_BLOCK, 0.0f);
                }
            }

            /// @brief Returns the pointer table.
            float* const* pointers()
            {
                return pointers_.data();
            }

            /// @brief Returns one channel.
            const std::vector<float>& channel(std::uint32_t index) const
            {
                return buffers_[index];
            }

        private:
            /// @brief Stores the sixteen buffers.
            std::array<std::vector<float>, K_CHANNELS> buffers_;

            /// @brief Stores one pointer per buffer.
            std::array<float*, K_CHANNELS> pointers_;
    };

    /// @brief Holds one count of taps rendered together.
    struct CountRow
    {
            const char* name;
            std::uint32_t count;
    };

    /// @brief Holds one output channel with whether the impulse is encoded into it.
    struct ChannelRow
    {
            const char* name;
            std::uint32_t channel;
            bool carries_impulse;
    };

    /// @brief Names a row for gtest.
    template <typename Row>
    std::string row_name(const ::testing::TestParamInfo<Row>& info)
    {
        return info.param.name;
    }

    /// @brief Returns a tap with a pass-through filter and an encode of 1 on channel 0.
    EarlyTap unit_tap(float delay_frames, float gain)
    {
        EarlyTap tap{};
        tap.delay_frames = delay_frames;
        tap.gain = gain;
        tap.section_a = {1.0, 0.0, 0.0, 0.0, 0.0};
        tap.section_b = {1.0, 0.0, 0.0, 0.0, 0.0};
        tap.encode.fill(0.0f);
        tap.encode[0] = 1.0f;
        return tap;
    }

    /// @brief Returns a set holding one unit tap.
    EarlyTapSet single_tap_set(float delay_frames)
    {
        EarlyTapSet set{};
        set.count = 1u;
        set.taps[0] = unit_tap(delay_frames, 1.0f);
        return set;
    }

    /// @brief Returns a tap that rings like a resonator near 1 kHz.
    EarlyTap ringing_tap(float delay_frames)
    {
        const double theta = 2.0 * K_PI * 1000.0 / 48000.0;
        EarlyTap tap = unit_tap(delay_frames, 1.0f);
        tap.section_a = {1.0, 0.0, 0.0, -2.0 * K_RING_RADIUS * std::cos(theta), K_RING_RADIUS * K_RING_RADIUS};
        return tap;
    }

    /// @brief Returns a prepared renderer.
    EarlyReflectionRenderer prepared_renderer()
    {
        EarlyReflectionRenderer renderer;
        renderer.prepare(K_MAX_DELAY, K_BLOCK);
        return renderer;
    }

    /// @brief Returns a prepared ring for a renderer to read.
    DelayRing prepared_ring()
    {
        DelayRing ring;
        ring.prepare(K_MAX_DELAY, K_BLOCK);
        return ring;
    }

    /// @brief Returns one block that is a unit impulse at frame 0.
    std::vector<float> impulse_block()
    {
        std::vector<float> block(K_BLOCK, 0.0f);
        block[0] = 1.0f;
        return block;
    }

    /// @brief Returns one block of silence.
    std::vector<float> silent_block()
    {
        return std::vector<float>(K_BLOCK, 0.0f);
    }

    /// @brief Returns one block of silence with a value at one frame.
    std::vector<float> block_with(std::uint32_t frame, float value)
    {
        std::vector<float> block = silent_block();
        block[frame] = value;
        return block;
    }

    /// @brief Writes an impulse block to a fresh ring, renders @p set from bank 0 and returns the
    /// accumulation buffers.
    OutputBlock render_impulse(const EarlyTapSet& set)
    {
        EarlyReflectionRenderer renderer = prepared_renderer();
        DelayRing ring = prepared_ring();
        const std::vector<float> input = impulse_block();
        ring.write(input.data(), K_BLOCK);
        OutputBlock outputs;
        renderer.render(ring, set, 0u, outputs.pointers(), K_BLOCK);
        return outputs;
    }

    /// @brief Renders a 1 kHz sine through one set and returns channel 0 over several blocks.
    std::vector<float> render_sine(const EarlyTapSet& set, std::uint32_t blocks)
    {
        EarlyReflectionRenderer renderer = prepared_renderer();
        DelayRing renderer_ring = prepared_ring();
        std::vector<float> input(K_BLOCK);
        std::vector<float> output;
        output.reserve(static_cast<std::size_t>(blocks) * K_BLOCK);
        for (std::uint32_t block = 0; block < blocks; ++block)
        {
            for (std::uint32_t frame = 0; frame < K_BLOCK; ++frame)
            {
                const double time = static_cast<double>(block * K_BLOCK + frame) / K_SAMPLE_RATE;
                input[frame] = static_cast<float>(std::sin(2.0 * K_PI * K_SINE_HZ * time));
            }
            renderer_ring.write(input.data(), K_BLOCK);
            OutputBlock outputs;
            renderer.render(renderer_ring, set, 0u, outputs.pointers(), K_BLOCK);
            output.insert(output.end(), outputs.channel(0).begin(), outputs.channel(0).end());
        }
        return output;
    }

    /// @brief Returns true when any of the first @p frames of @p samples is not zero.
    bool has_nonzero(const std::vector<float>& samples, std::uint32_t frames)
    {
        for (std::uint32_t frame = 0; frame < frames; ++frame)
        {
            if (samples[frame] != 0.0f)
            {
                return true;
            }
        }
        return false;
    }

    /// @brief Expects the frames of @p output from @p first on to follow the sine delayed by
    /// @p delay_frames within K_HALF_FRAME_BOUND.
    void expect_delayed_sine(const std::vector<float>& output, double delay_frames, std::size_t first)
    {
        for (std::size_t frame = first; frame < output.size(); ++frame)
        {
            const double ideal = std::sin(2.0 * K_PI * K_SINE_HZ * (static_cast<double>(frame) - delay_frames) / K_SAMPLE_RATE);
            EXPECT_TRUE(TestSupport::is_within(
                TestSupport::measure_error(output[frame], ideal, K_HALF_FRAME_BOUND, 1.0)))
                << "frame " << frame;
        }
    }

    /// @brief Holds a renderer whose bank 0 has rung from an impulse for ten blocks, with the ring
    /// holding the impulse of the next block and the first block bank 0 gave.
    struct RingingBankZero
    {
            /// @brief Stores the renderer, ringing in bank 0.
            EarlyReflectionRenderer renderer = prepared_renderer();

            /// @brief Stores the ring, holding an impulse as its last block.
            DelayRing ring = prepared_ring();

            /// @brief Stores the set rendered, one ringing tap.
            EarlyTapSet set = []
            {
                EarlyTapSet ringing{};
                ringing.count = 1u;
                ringing.taps[0] = ringing_tap(100.0f);
                return ringing;
            }();

            /// @brief Stores channel 0 of the first block bank 0 rendered.
            std::vector<float> first_block;

            /// @brief Rings bank 0 for ten blocks and writes an impulse block last.
            RingingBankZero()
            {
                const std::vector<float> impulse = impulse_block();
                const std::vector<float> silence = silent_block();
                ring.write(impulse.data(), K_BLOCK);
                OutputBlock first;
                renderer.render(ring, set, 0u, first.pointers(), K_BLOCK);
                first_block = first.channel(0);
                OutputBlock scratch;
                for (int block = 1; block < 10; ++block)
                {
                    ring.write(silence.data(), K_BLOCK);
                    scratch.clear();
                    renderer.render(ring, set, 0u, scratch.pointers(), K_BLOCK);
                }
                ring.write(impulse.data(), K_BLOCK);
            }
    };

    /// @brief Expects an alone-rendered tap of a group to match the group's channel for it, for a
    /// group of @p count taps that each encode to a channel of their own.
    void expect_group_matches_each_tap_alone(std::uint32_t count)
    {
        EarlyTapSet group{};
        group.count = count;
        for (std::uint32_t index = 0; index < count; ++index)
        {
            EarlyTap tap = unit_tap(3.25f + 11.5f * static_cast<float>(index), 0.5f + 0.1f * index);
            tap.section_a = {0.6, 0.25, 0.05, -0.3 + 0.03 * index, 0.1};
            tap.section_b = {0.9, -0.2, 0.0, 0.2 - 0.02 * index, 0.0};
            tap.encode.fill(0.0f);
            tap.encode[index] = 1.0f;
            group.taps[index] = tap;
        }
        EarlyReflectionRenderer together = prepared_renderer();
        DelayRing ring = prepared_ring();
        const std::vector<float> impulse = impulse_block();
        ring.write(impulse.data(), K_BLOCK);
        OutputBlock grouped;
        together.render(ring, group, 0u, grouped.pointers(), K_BLOCK);

        for (std::uint32_t index = 0; index < count; ++index)
        {
            EarlyTapSet single{};
            single.count = 1u;
            single.taps[0] = group.taps[index];
            EarlyReflectionRenderer alone = prepared_renderer();
            OutputBlock lone;
            alone.render(ring, single, 0u, lone.pointers(), K_BLOCK);
            EXPECT_EQ(grouped.channel(index), lone.channel(index)) << "count " << count << " tap " << index;
        }
    }

    /// @brief Fills a block with a 220 Hz tone at 0.3 of full scale.
    std::vector<float> room_tone()
    {
        std::vector<float> block(256);
        for (std::size_t i = 0; i < block.size(); ++i)
        {
            block[i] = 0.3f * std::sin(2.0f * static_cast<float>(K_PI) * 220.0f * static_cast<float>(i) / 48000.0f);
        }
        return block;
    }

    /// @brief Returns the hundred and twenty-eight taps the allocation test renders.
    EarlyTapSet full_tap_set()
    {
        EarlyTapSet set{};
        set.count = EarlyTapSet::K_MAX_EARLY_TAPS;
        for (std::uint32_t tap = 0; tap < set.count; ++tap)
        {
            set.taps[tap].delay_frames = 1.5f + 60.0f * static_cast<float>(tap);
            set.taps[tap].gain = 1.0f / (1.0f + static_cast<float>(tap));
            set.taps[tap].section_a = {0.5, 0.25, 0.0, -0.25, 0.0};
            set.taps[tap].section_b = {1.0, 0.0, 0.0, 0.0, 0.0};
            set.taps[tap].encode.fill(0.1f);
        }
        return set;
    }

    /// @brief Runs eight blocks of writing, rendering from both banks, carrying and clearing.
    void run_blocks_with_bank_changes(EarlyReflectionRenderer& renderer, DelayRing& ring, const EarlyTapSet& set,
                                      const std::vector<float>& input, OutputBlock& outputs)
    {
        for (int block = 0; block < 8; ++block)
        {
            ring.write(input.data(), 256);
            renderer.render(ring, set, 0, outputs.pointers(), 256);
            renderer.render(ring, set, 1, outputs.pointers(), 256);
            renderer.carry_tap(0, 1, static_cast<std::uint32_t>(block));
            renderer.clear_bank(static_cast<std::uint32_t>(block % 2));
        }
    }

    /// @brief Expects every sample of @p samples to be finite.
    void expect_all_finite(const std::vector<float>& samples)
    {
        for (const float sample : samples)
        {
            EXPECT_TRUE(std::isfinite(sample));
        }
    }

    class Small_EarlyReflectionRendererGroup : public ::testing::TestWithParam<CountRow>
    {
    };

    class Small_EarlyReflectionRendererChannel : public ::testing::TestWithParam<ChannelRow>
    {
    };
} // namespace

TEST(Small_EarlyReflectionRenderer, EarlyReflectionRendererPutsAnImpulseAtTheTapsDelay)
{
    // A whole delay reads the ring at the exact Lagrange weights 0, 0, 1, 0 through a pass-through filter.
    const OutputBlock outputs = render_impulse(single_tap_set(100.0f));

    EXPECT_EQ(outputs.channel(0), block_with(100u, 1.0f));
}

TEST(Small_EarlyReflectionRenderer, EarlyReflectionRendererSplitsAnImpulseAtAHalfFrameDelayIntoTwoNineSixteenths)
{
    const OutputBlock outputs = render_impulse(single_tap_set(100.5f));

    // The half-frame cubic Lagrange weights are -1/16, 9/16, 9/16 and -1/16.
    EXPECT_TRUE(TestSupport::is_within(
        TestSupport::measure_relative_error<float>(outputs.channel(0)[100], 0.5625, K_KERNEL_OPERATIONS)));
    EXPECT_TRUE(TestSupport::is_within(
        TestSupport::measure_relative_error<float>(outputs.channel(0)[101], 0.5625, K_KERNEL_OPERATIONS)));
}

TEST(Small_EarlyReflectionRenderer, EarlyReflectionRendererDelaysASineByAHalfFrameToTheCubicLagrangeBound)
{
    // The first 512 frames are skipped so that the delay line has filled with the sine.
    expect_delayed_sine(render_sine(single_tap_set(100.5f), 16u), 100.5, 512u);
}

TEST(Small_EarlyReflectionRenderer, EarlyReflectionRendererSumsOneHundredAndTwentyEightTaps)
{
    // Each tap is a whole delay of gain 1/128, exact in float, so every frame in the run is 1/128.
    EarlyTapSet set{};
    set.count = static_cast<std::uint32_t>(EarlyTapSet::K_MAX_EARLY_TAPS);
    std::vector<float> expected = silent_block();
    for (std::uint32_t tap = 0; tap < set.count; ++tap)
    {
        set.taps[tap] = unit_tap(static_cast<float>(100u + tap), 1.0f / 128.0f);
        expected[100u + tap] = 1.0f / 128.0f;
    }

    const OutputBlock outputs = render_impulse(set);

    EXPECT_EQ(outputs.channel(0), expected);
}

TEST_P(Small_EarlyReflectionRendererChannel, EarlyReflectionRendererEncodesTheImpulseOnlyIntoItsChannels)
{
    EarlyTapSet set = single_tap_set(100.0f);
    set.taps[0].encode[3] = 1.0f;
    const ChannelRow row = GetParam();

    const OutputBlock outputs = render_impulse(set);

    EXPECT_EQ(outputs.channel(row.channel), row.carries_impulse ? block_with(100u, 1.0f) : silent_block());
}

INSTANTIATE_TEST_SUITE_P(
    Channels, Small_EarlyReflectionRendererChannel,
    ::testing::Values(ChannelRow{"Channel0", 0u, true}, ChannelRow{"Channel1", 1u, false},
                      ChannelRow{"Channel2", 2u, false}, ChannelRow{"Channel3", 3u, true},
                      ChannelRow{"Channel4", 4u, false}, ChannelRow{"Channel5", 5u, false},
                      ChannelRow{"Channel6", 6u, false}, ChannelRow{"Channel7", 7u, false},
                      ChannelRow{"Channel8", 8u, false}, ChannelRow{"Channel9", 9u, false},
                      ChannelRow{"Channel10", 10u, false}, ChannelRow{"Channel11", 11u, false},
                      ChannelRow{"Channel12", 12u, false}, ChannelRow{"Channel13", 13u, false},
                      ChannelRow{"Channel14", 14u, false}, ChannelRow{"Channel15", 15u, false}),
    row_name<ChannelRow>);

TEST(Small_EarlyReflectionRenderer, EarlyReflectionRendererGivesABankTheSameFirstBlockWhateverAnotherBankHasRung)
{
    // Both banks start from zero state and read the same ring block with the same arithmetic.
    RingingBankZero scene;

    scene.renderer.clear_bank(1u);
    OutputBlock from_bank_one;
    scene.renderer.render(scene.ring, scene.set, 1u, from_bank_one.pointers(), K_BLOCK);

    EXPECT_EQ(from_bank_one.channel(0), scene.first_block);
}

TEST(Small_EarlyReflectionRenderer, EarlyReflectionRendererLeavesTheOtherBankRingingWhenOneIsCleared)
{
    RingingBankZero scene;

    scene.renderer.clear_bank(1u);
    OutputBlock ringing_bank_zero;
    scene.renderer.render(scene.ring, scene.set, 0u, ringing_bank_zero.pointers(), K_BLOCK);

    EXPECT_TRUE(has_nonzero(ringing_bank_zero.channel(0), 100u));
}

TEST(Small_EarlyReflectionRenderer, EarlyReflectionRendererCarriesOneTapsStateBetweenBanks)
{
    // Only tap 0's state is carried into bank 1. See tests/nodes/room/README.md.
    EarlyReflectionRenderer renderer = prepared_renderer();
    DelayRing ring = prepared_ring();
    EarlyTapSet both{};
    both.count = 2u;
    both.taps[0] = ringing_tap(100.0f);
    both.taps[1] = ringing_tap(200.0f);
    EarlyTapSet first_only = both;
    first_only.count = 1u;
    const std::vector<float> impulse = impulse_block();
    const std::vector<float> silence = silent_block();
    ring.write(impulse.data(), K_BLOCK);
    OutputBlock scratch;
    renderer.render(ring, both, 0u, scratch.pointers(), K_BLOCK);
    for (int block = 1; block < 10; ++block)
    {
        ring.write(silence.data(), K_BLOCK);
        scratch.clear();
        renderer.render(ring, both, 0u, scratch.pointers(), K_BLOCK);
    }

    renderer.carry_tap(0u, 1u, 0u);
    ring.write(silence.data(), K_BLOCK);
    OutputBlock from_bank_zero;
    renderer.render(ring, first_only, 0u, from_bank_zero.pointers(), K_BLOCK);
    OutputBlock from_bank_one;
    renderer.render(ring, both, 1u, from_bank_one.pointers(), K_BLOCK);

    EXPECT_EQ(from_bank_one.channel(0), from_bank_zero.channel(0));
    EXPECT_TRUE(has_nonzero(from_bank_one.channel(0), K_BLOCK));
}

TEST(Small_EarlyReflectionRenderer, EarlyReflectionRendererCarriesATapOntoItselfWithoutAChange)
{
    // A carry onto the same bank is a copy onto itself. See tests/nodes/room/README.md.
    EarlyReflectionRenderer renderer = prepared_renderer();
    DelayRing ring = prepared_ring();
    EarlyTapSet set{};
    set.count = 1u;
    set.taps[0] = ringing_tap(100.0f);
    const std::vector<float> impulse = impulse_block();
    const std::vector<float> silence = silent_block();
    ring.write(impulse.data(), K_BLOCK);
    OutputBlock scratch;
    renderer.render(ring, set, 0u, scratch.pointers(), K_BLOCK);
    EarlyReflectionRenderer untouched = renderer;
    DelayRing untouched_ring = ring;

    renderer.carry_tap(0u, 0u, 0u);
    ring.write(silence.data(), K_BLOCK);
    untouched_ring.write(silence.data(), K_BLOCK);
    OutputBlock carried;
    renderer.render(ring, set, 0u, carried.pointers(), K_BLOCK);
    OutputBlock reference;
    untouched.render(untouched_ring, set, 0u, reference.pointers(), K_BLOCK);

    EXPECT_EQ(carried.channel(0), reference.channel(0));
}

TEST(Small_EarlyReflectionRenderer, EarlyReflectionRendererRendersNothingInTheBlockBeforeALongDelayArrives)
{
    EarlyReflectionRenderer renderer = prepared_renderer();
    DelayRing ring = prepared_ring();
    const EarlyTapSet set = single_tap_set(300.0f);
    const std::vector<float> impulse = impulse_block();
    ring.write(impulse.data(), K_BLOCK);
    OutputBlock first;

    renderer.render(ring, set, 0u, first.pointers(), K_BLOCK);

    EXPECT_EQ(first.channel(0), silent_block());
}

TEST(Small_EarlyReflectionRenderer, EarlyReflectionRendererCarriesATailAcrossBlockBoundaries)
{
    EarlyReflectionRenderer renderer = prepared_renderer();
    DelayRing ring = prepared_ring();
    const EarlyTapSet set = single_tap_set(300.0f);
    const std::vector<float> impulse = impulse_block();
    const std::vector<float> silence = silent_block();
    ring.write(impulse.data(), K_BLOCK);
    OutputBlock first;
    renderer.render(ring, set, 0u, first.pointers(), K_BLOCK);
    ring.write(silence.data(), K_BLOCK);
    OutputBlock second;

    renderer.render(ring, set, 0u, second.pointers(), K_BLOCK);

    // The impulse written 256 frames ago arrives 300 - 256 = 44 frames into this block.
    EXPECT_EQ(second.channel(0), block_with(44u, 1.0f));
}

TEST_P(Small_EarlyReflectionRendererGroup, EarlyReflectionRendererRendersATapTheSameAloneAndInAGroup)
{
    // Each tap encodes to a channel of its own, so the group's lanes can be read apart.
    expect_group_matches_each_tap_alone(GetParam().count);
}

INSTANTIATE_TEST_SUITE_P(Counts, Small_EarlyReflectionRendererGroup,
                         ::testing::Values(CountRow{"One", 1u}, CountRow{"Four", 4u}, CountRow{"Five", 5u},
                                           CountRow{"Seven", 7u}, CountRow{"Sixteen", 16u}),
                         row_name<CountRow>);

TEST(Medium_EarlyReflectionRenderer, RenderPerformsNoHeapAllocation)
{
    EarlyReflectionRenderer renderer;
    renderer.prepare(8397, 256);
    DelayRing renderer_ring;
    renderer_ring.prepare(8397, 256);
    const EarlyTapSet set = full_tap_set();
    const std::vector<float> input = room_tone();
    OutputBlock outputs;

    SushiDSP::test::AllocationTrap trap;
    trap.arm();
    run_blocks_with_bank_changes(renderer, renderer_ring, set, input, outputs);
    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
    expect_all_finite(outputs.channel(0));
}
