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
 * @file test_early_reflection_renderer.cpp
 * @brief Tests for EarlyReflectionRenderer, driven by hand-written tap sets with no solver.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <SushiDSP/foundation/allocation_trap/allocation_trap.hpp>
#include <SushiDSP/nodes/room/delay_ring.hpp>
#include <SushiDSP/nodes/room/early_reflection_renderer.hpp>
#include <SushiDSP/nodes/room/early_tap_set.hpp>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{
    /// @brief Counts the channels the renderer accumulates into.
    constexpr std::uint32_t K_CHANNELS = SushiDSP::room::EarlyReflectionRenderer::K_OUTPUT_CHANNELS;

    /// @brief Holds the block length every test renders in.
    constexpr std::uint32_t K_BLOCK = 256;

    /// @brief Holds the longest delay the tests ask the ring to hold.
    constexpr std::uint32_t K_MAX_DELAY = 8397;

    /// @brief Holds pi.
    constexpr double K_PI = 3.14159265358979323846;

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

    /// @brief Returns a tap with a pass-through filter and an encode of 1 on channel 0.
    SushiDSP::room::EarlyTap unit_tap(float delay_frames, float gain)
    {
        SushiDSP::room::EarlyTap tap{};
        tap.delay_frames = delay_frames;
        tap.gain = gain;
        tap.section_a = {1.0, 0.0, 0.0, 0.0, 0.0};
        tap.section_b = {1.0, 0.0, 0.0, 0.0, 0.0};
        tap.encode.fill(0.0f);
        tap.encode[0] = 1.0f;
        return tap;
    }

    /// @brief Returns a set holding one unit tap.
    SushiDSP::room::EarlyTapSet single_tap_set(float delay_frames)
    {
        SushiDSP::room::EarlyTapSet set{};
        set.count = 1u;
        set.taps[0] = unit_tap(delay_frames, 1.0f);
        return set;
    }

    /// @brief Returns a prepared renderer.
    SushiDSP::room::EarlyReflectionRenderer prepared_renderer()
    {
        SushiDSP::room::EarlyReflectionRenderer renderer;
        renderer.prepare(K_MAX_DELAY, K_BLOCK);
        return renderer;
    }

    /// @brief Returns a prepared ring for a renderer to read.
    SushiDSP::room::DelayRing prepared_ring()
    {
        SushiDSP::room::DelayRing ring;
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

    /// @brief Renders a 1 kHz sine through one set and returns channel 0 over several blocks.
    std::vector<float> render_sine(const SushiDSP::room::EarlyTapSet& set, std::uint32_t blocks)
    {
        SushiDSP::room::EarlyReflectionRenderer renderer = prepared_renderer();
        SushiDSP::room::DelayRing renderer_ring = prepared_ring();
        std::vector<float> input(K_BLOCK);
        std::vector<float> output;
        output.reserve(static_cast<std::size_t>(blocks) * K_BLOCK);
        for (std::uint32_t block = 0; block < blocks; ++block)
        {
            for (std::uint32_t frame = 0; frame < K_BLOCK; ++frame)
            {
                const double time = static_cast<double>(block * K_BLOCK + frame) / 48000.0;
                input[frame] = static_cast<float>(std::sin(2.0 * K_PI * 1000.0 * time));
            }
            renderer_ring.write(input.data(), K_BLOCK);
            OutputBlock outputs;
            renderer.render(renderer_ring, set, 0u, outputs.pointers(), K_BLOCK);
            output.insert(output.end(), outputs.channel(0).begin(), outputs.channel(0).end());
        }
        return output;
    }

    /// @brief Returns how many frames late lags early by, from the peak of their
    /// cross-correlation over integer lags refined by a parabola through its neighbours.
    double lag_frames(const std::vector<float>& early, const std::vector<float>& late,
                      std::ptrdiff_t start, std::ptrdiff_t length)
    {
        std::array<double, 5> correlation{};
        for (std::ptrdiff_t lag = -2; lag <= 2; ++lag)
        {
            double sum = 0.0;
            for (std::ptrdiff_t frame = start; frame < start + length; ++frame)
            {
                sum += static_cast<double>(late[static_cast<std::size_t>(frame)]) *
                       static_cast<double>(early[static_cast<std::size_t>(frame - lag)]);
            }
            correlation[static_cast<std::size_t>(lag + 2)] = sum;
        }
        std::size_t peak = 1;
        for (std::size_t index = 2; index + 1 < correlation.size(); ++index)
        {
            if (correlation[index] > correlation[peak])
            {
                peak = index;
            }
        }
        const double left = correlation[peak - 1];
        const double centre = correlation[peak];
        const double right = correlation[peak + 1];
        const double offset = 0.5 * (left - right) / (left - 2.0 * centre + right);
        return static_cast<double>(peak) - 2.0 + offset;
    }
} // namespace

TEST(EarlyReflectionRenderer, EarlyReflectionRendererPutsAnImpulseAtTheTapsDelay)
{
    SushiDSP::room::EarlyReflectionRenderer renderer = prepared_renderer();
    SushiDSP::room::DelayRing renderer_ring = prepared_ring();
    const SushiDSP::room::EarlyTapSet set = single_tap_set(100.0f);
    const std::vector<float> input = impulse_block();
    renderer_ring.write(input.data(), K_BLOCK);
    OutputBlock outputs;
    renderer.render(renderer_ring, set, 0u, outputs.pointers(), K_BLOCK);
    for (std::uint32_t frame = 0; frame < K_BLOCK; ++frame)
    {
        if (frame == 100u)
        {
            EXPECT_EQ(outputs.channel(0)[frame], 1.0f);
        }
        else
        {
            EXPECT_LT(std::fabs(outputs.channel(0)[frame]), 1e-6f) << "frame " << frame;
        }
    }
}

TEST(EarlyReflectionRenderer, EarlyReflectionRendererInterpolatesAFractionalDelay)
{
    SushiDSP::room::EarlyReflectionRenderer renderer = prepared_renderer();
    SushiDSP::room::DelayRing renderer_ring = prepared_ring();
    const SushiDSP::room::EarlyTapSet half = single_tap_set(100.5f);
    const std::vector<float> input = impulse_block();
    renderer_ring.write(input.data(), K_BLOCK);
    OutputBlock outputs;
    renderer.render(renderer_ring, half, 0u, outputs.pointers(), K_BLOCK);
    EXPECT_GT(outputs.channel(0)[100], 0.4f);
    EXPECT_GT(outputs.channel(0)[101], 0.4f);

    const std::vector<float> whole_sine = render_sine(single_tap_set(100.0f), 16u);
    const std::vector<float> half_sine = render_sine(half, 16u);
    // 2016 frames are 42 whole periods of the 1 kHz sine, so the correlation is symmetric.
    EXPECT_NEAR(lag_frames(whole_sine, half_sine, 512, 2016), 0.5, 0.01);
}

TEST(EarlyReflectionRenderer, EarlyReflectionRendererSumsOneHundredAndTwentyEightTaps)
{
    SushiDSP::room::EarlyReflectionRenderer renderer = prepared_renderer();
    SushiDSP::room::DelayRing renderer_ring = prepared_ring();
    SushiDSP::room::EarlyTapSet set{};
    set.count = static_cast<std::uint32_t>(SushiDSP::room::EarlyTapSet::K_MAX_EARLY_TAPS);
    for (std::uint32_t tap = 0; tap < set.count; ++tap)
    {
        set.taps[tap] = unit_tap(static_cast<float>(100u + tap), 1.0f / 128.0f);
    }
    const std::vector<float> input = impulse_block();
    renderer_ring.write(input.data(), K_BLOCK);
    OutputBlock outputs;
    renderer.render(renderer_ring, set, 0u, outputs.pointers(), K_BLOCK);
    double sum = 0.0;
    for (std::uint32_t frame = 0; frame < K_BLOCK; ++frame)
    {
        const float value = outputs.channel(0)[frame];
        sum += static_cast<double>(value);
        if (frame >= 100u && frame < 228u)
        {
            EXPECT_NEAR(value, 1.0f / 128.0f, 1e-7f) << "frame " << frame;
        }
        else
        {
            EXPECT_LT(std::fabs(value), 1e-9f) << "frame " << frame;
        }
    }
    EXPECT_NEAR(sum, 1.0, 1e-5);
}

TEST(EarlyReflectionRenderer, EarlyReflectionRendererEncodesIntoSixteenChannels)
{
    SushiDSP::room::EarlyReflectionRenderer renderer = prepared_renderer();
    SushiDSP::room::DelayRing renderer_ring = prepared_ring();
    SushiDSP::room::EarlyTapSet set = single_tap_set(100.0f);
    set.taps[0].encode[3] = 1.0f;
    const std::vector<float> input = impulse_block();
    renderer_ring.write(input.data(), K_BLOCK);
    OutputBlock outputs;
    renderer.render(renderer_ring, set, 0u, outputs.pointers(), K_BLOCK);
    for (std::uint32_t channel = 0; channel < K_CHANNELS; ++channel)
    {
        if (channel == 0u || channel == 3u)
        {
            EXPECT_EQ(outputs.channel(channel)[100], 1.0f) << "channel " << channel;
            continue;
        }
        for (std::uint32_t frame = 0; frame < K_BLOCK; ++frame)
        {
            EXPECT_LT(std::fabs(outputs.channel(channel)[frame]), 1e-9f)
                << "channel " << channel << " frame " << frame;
        }
    }
}

TEST(EarlyReflectionRenderer, EarlyReflectionRendererKeepsTwoIndependentStateBanks)
{
    SushiDSP::room::EarlyReflectionRenderer renderer = prepared_renderer();
    SushiDSP::room::DelayRing renderer_ring = prepared_ring();
    SushiDSP::room::EarlyTapSet set = single_tap_set(100.0f);
    // A resonator near 1 kHz with a pole radius of 0.999 rings for many blocks.
    const double radius = 0.999;
    const double theta = 2.0 * K_PI * 1000.0 / 48000.0;
    set.taps[0].section_a = {1.0, 0.0, 0.0, -2.0 * radius * std::cos(theta), radius * radius};

    const std::vector<float> impulse = impulse_block();
    const std::vector<float> silence(K_BLOCK, 0.0f);
    renderer_ring.write(impulse.data(), K_BLOCK);
    OutputBlock first_from_bank_zero;
    renderer.render(renderer_ring, set, 0u, first_from_bank_zero.pointers(), K_BLOCK);
    OutputBlock scratch;
    for (int block = 1; block < 10; ++block)
    {
        renderer_ring.write(silence.data(), K_BLOCK);
        scratch.clear();
        renderer.render(renderer_ring, set, 0u, scratch.pointers(), K_BLOCK);
    }

    renderer_ring.write(impulse.data(), K_BLOCK);
    renderer.clear_bank(1u);
    OutputBlock first_from_bank_one;
    renderer.render(renderer_ring, set, 1u, first_from_bank_one.pointers(), K_BLOCK);
    for (std::uint32_t frame = 0; frame < K_BLOCK; ++frame)
    {
        EXPECT_EQ(first_from_bank_one.channel(0)[frame], first_from_bank_zero.channel(0)[frame])
            << "frame " << frame;
    }

    // Bank 0 still rings from its own history, so the same block through it is not bank 1's.
    OutputBlock ringing_bank_zero;
    renderer.render(renderer_ring, set, 0u, ringing_bank_zero.pointers(), K_BLOCK);
    bool differs = false;
    for (std::uint32_t frame = 0; frame < 100u; ++frame)
    {
        differs = differs || ringing_bank_zero.channel(0)[frame] != 0.0f;
    }
    EXPECT_TRUE(differs);
}

TEST(EarlyReflectionRenderer, EarlyReflectionRendererCarriesOneTapsStateBetweenBanks)
{
    // Only tap 0's state is carried into bank 1. See tests/nodes/room/README.md.
    SushiDSP::room::EarlyReflectionRenderer renderer = prepared_renderer();
    SushiDSP::room::DelayRing renderer_ring = prepared_ring();
    const double radius = 0.999;
    const double theta = 2.0 * K_PI * 1000.0 / 48000.0;
    SushiDSP::room::EarlyTapSet both = single_tap_set(100.0f);
    both.taps[0].section_a = {1.0, 0.0, 0.0, -2.0 * radius * std::cos(theta), radius * radius};
    both.count = 2u;
    both.taps[1] = both.taps[0];
    both.taps[1].delay_frames = 200.0f;
    SushiDSP::room::EarlyTapSet first_only = both;
    first_only.count = 1u;

    const std::vector<float> impulse = impulse_block();
    const std::vector<float> silence(K_BLOCK, 0.0f);
    renderer_ring.write(impulse.data(), K_BLOCK);
    OutputBlock scratch;
    renderer.render(renderer_ring, both, 0u, scratch.pointers(), K_BLOCK);
    for (int block = 1; block < 10; ++block)
    {
        renderer_ring.write(silence.data(), K_BLOCK);
        scratch.clear();
        renderer.render(renderer_ring, both, 0u, scratch.pointers(), K_BLOCK);
    }

    renderer.carry_tap(0u, 1u, 0u);
    renderer_ring.write(silence.data(), K_BLOCK);
    OutputBlock from_bank_zero;
    renderer.render(renderer_ring, first_only, 0u, from_bank_zero.pointers(), K_BLOCK);
    OutputBlock from_bank_one;
    renderer.render(renderer_ring, both, 1u, from_bank_one.pointers(), K_BLOCK);
    bool rings = false;
    for (std::uint32_t frame = 0; frame < K_BLOCK; ++frame)
    {
        EXPECT_EQ(from_bank_one.channel(0)[frame], from_bank_zero.channel(0)[frame])
            << "frame " << frame;
        rings = rings || from_bank_one.channel(0)[frame] != 0.0f;
    }
    EXPECT_TRUE(rings);
}

TEST(EarlyReflectionRenderer, EarlyReflectionRendererCarriesATapOntoItselfWithoutAChange)
{
    // A carry onto the same bank is a copy onto itself. See tests/nodes/room/README.md.
    SushiDSP::room::EarlyReflectionRenderer renderer = prepared_renderer();
    SushiDSP::room::DelayRing renderer_ring = prepared_ring();
    const double radius = 0.999;
    const double theta = 2.0 * K_PI * 1000.0 / 48000.0;
    SushiDSP::room::EarlyTapSet set = single_tap_set(100.0f);
    set.taps[0].section_a = {1.0, 0.0, 0.0, -2.0 * radius * std::cos(theta), radius * radius};

    const std::vector<float> impulse = impulse_block();
    const std::vector<float> silence(K_BLOCK, 0.0f);
    renderer_ring.write(impulse.data(), K_BLOCK);
    OutputBlock scratch;
    renderer.render(renderer_ring, set, 0u, scratch.pointers(), K_BLOCK);

    SushiDSP::room::EarlyReflectionRenderer untouched = renderer;

    SushiDSP::room::DelayRing untouched_ring = renderer_ring;
    renderer.carry_tap(0u, 0u, 0u);
    renderer_ring.write(silence.data(), K_BLOCK);
    untouched_ring.write(silence.data(), K_BLOCK);
    OutputBlock carried;
    renderer.render(renderer_ring, set, 0u, carried.pointers(), K_BLOCK);
    OutputBlock reference;
    untouched.render(untouched_ring, set, 0u, reference.pointers(), K_BLOCK);
    for (std::uint32_t frame = 0; frame < K_BLOCK; ++frame)
    {
        EXPECT_EQ(carried.channel(0)[frame], reference.channel(0)[frame]) << "frame " << frame;
    }
}

TEST(EarlyReflectionRenderer, EarlyReflectionRendererCarriesATailAcrossBlockBoundaries)
{
    SushiDSP::room::EarlyReflectionRenderer renderer = prepared_renderer();
    SushiDSP::room::DelayRing renderer_ring = prepared_ring();
    const SushiDSP::room::EarlyTapSet set = single_tap_set(300.0f);
    const std::vector<float> impulse = impulse_block();
    const std::vector<float> silence(K_BLOCK, 0.0f);

    renderer_ring.write(impulse.data(), K_BLOCK);
    OutputBlock first;
    renderer.render(renderer_ring, set, 0u, first.pointers(), K_BLOCK);
    for (std::uint32_t frame = 0; frame < K_BLOCK; ++frame)
    {
        EXPECT_LT(std::fabs(first.channel(0)[frame]), 1e-9f) << "frame " << frame;
    }

    renderer_ring.write(silence.data(), K_BLOCK);
    OutputBlock second;
    renderer.render(renderer_ring, set, 0u, second.pointers(), K_BLOCK);
    for (std::uint32_t frame = 0; frame < K_BLOCK; ++frame)
    {
        if (frame == 44u)
        {
            EXPECT_EQ(second.channel(0)[frame], 1.0f);
        }
        else
        {
            EXPECT_LT(std::fabs(second.channel(0)[frame]), 1e-9f) << "frame " << frame;
        }
    }
}

TEST(EarlyReflectionRenderer, EarlyReflectionRendererRendersATapTheSameAloneAndInAGroup)
{
    // Each tap encodes to a channel of its own, so the group's lanes can be read apart.
    for (const std::uint32_t count : {1u, 4u, 5u, 7u, 16u})
    {
        SushiDSP::room::EarlyTapSet group{};
        group.count = count;
        for (std::uint32_t index = 0; index < count; ++index)
        {
            SushiDSP::room::EarlyTap tap =
                unit_tap(3.25f + 11.5f * static_cast<float>(index), 0.5f + 0.1f * index);
            tap.section_a = {0.6, 0.25, 0.05, -0.3 + 0.03 * index, 0.1};
            tap.section_b = {0.9, -0.2, 0.0, 0.2 - 0.02 * index, 0.0};
            tap.encode.fill(0.0f);
            tap.encode[index] = 1.0f;
            group.taps[index] = tap;
        }

        SushiDSP::room::EarlyReflectionRenderer together = prepared_renderer();
        SushiDSP::room::DelayRing ring = prepared_ring();
        const std::vector<float> impulse = impulse_block();
        ring.write(impulse.data(), K_BLOCK);
        OutputBlock grouped;
        together.render(ring, group, 0u, grouped.pointers(), K_BLOCK);

        for (std::uint32_t index = 0; index < count; ++index)
        {
            SushiDSP::room::EarlyTapSet single{};
            single.count = 1u;
            single.taps[0] = group.taps[index];
            SushiDSP::room::EarlyReflectionRenderer alone = prepared_renderer();
            OutputBlock lone;
            alone.render(ring, single, 0u, lone.pointers(), K_BLOCK);
            for (std::uint32_t frame = 0; frame < K_BLOCK; ++frame)
            {
                ASSERT_EQ(grouped.channel(index)[frame], lone.channel(index)[frame])
                    << "count " << count << " tap " << index << " frame " << frame;
            }
        }
    }
}

namespace
{
    /// @brief Fills a block with a 220 Hz tone at 0.3 of full scale.
    void fill_room_tone(std::vector<float>& block)
    {
        for (std::size_t i = 0; i < block.size(); ++i)
        {
            block[i] = 0.3f * std::sin(2.0f * static_cast<float>(M_PI) * 220.0f *
                                       static_cast<float>(i) / 48000.0f);
        }
    }
} // namespace

TEST(EarlyReflectionRenderer, RenderPerformsNoHeapAllocation)
{
    SushiDSP::room::EarlyReflectionRenderer renderer;
    renderer.prepare(8397, 256);
    SushiDSP::room::DelayRing renderer_ring;
    renderer_ring.prepare(8397, 256);

    SushiDSP::room::EarlyTapSet set{};
    set.count = SushiDSP::room::EarlyTapSet::K_MAX_EARLY_TAPS;
    for (std::uint32_t tap = 0; tap < set.count; ++tap)
    {
        set.taps[tap].delay_frames = 1.5f + 60.0f * static_cast<float>(tap);
        set.taps[tap].gain = 1.0f / (1.0f + static_cast<float>(tap));
        set.taps[tap].section_a = {0.5, 0.25, 0.0, -0.25, 0.0};
        set.taps[tap].section_b = {1.0, 0.0, 0.0, 0.0, 0.0};
        set.taps[tap].encode.fill(0.1f);
    }

    std::vector<float> input(256);
    fill_room_tone(input);
    std::array<std::vector<float>, SushiDSP::room::EarlyReflectionRenderer::K_OUTPUT_CHANNELS>
        buffers;
    std::array<float*, SushiDSP::room::EarlyReflectionRenderer::K_OUTPUT_CHANNELS> outputs{};
    for (std::size_t channel = 0; channel < buffers.size(); ++channel)
    {
        buffers[channel].assign(256, 0.0f);
        outputs[channel] = buffers[channel].data();
    }

    SushiDSP::test::AllocationTrap trap;
    trap.arm();
    for (int block = 0; block < 8; ++block)
    {
        renderer_ring.write(input.data(), 256);
        renderer.render(renderer_ring, set, 0, outputs.data(), 256);
        renderer.render(renderer_ring, set, 1, outputs.data(), 256);
        renderer.carry_tap(0, 1, static_cast<std::uint32_t>(block));
        renderer.clear_bank(static_cast<std::uint32_t>(block % 2));
    }
    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
    for (float sample : buffers[0])
    {
        EXPECT_TRUE(std::isfinite(sample));
    }
}
