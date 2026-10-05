/****************************************************************/
/* test_early_reflection_renderer.cpp                           */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

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

#include "SushiDSP/room/EarlyReflectionRenderer.hpp"

namespace
{
    /// @brief Counts the channels the renderer accumulates into.
    constexpr std::uint32_t kChannels = SushiDSP::room::EarlyReflectionRenderer::kOutputChannels;

    /// @brief Holds the block length every test renders in.
    constexpr std::uint32_t kBlock = 256;

    /// @brief Holds the longest delay the tests ask the ring to hold.
    constexpr std::uint32_t kMaxDelay = 8397;

    /// @brief Holds pi.
    constexpr double kPi = 3.14159265358979323846;

    /// @brief Holds sixteen accumulation buffers and the pointer table the renderer takes.
    class OutputBlock
    {
    public:
        /// @brief Allocates zeroed buffers of one block each.
        OutputBlock()
        {
            for (std::uint32_t channel = 0; channel < kChannels; ++channel)
            {
                buffers_[channel].assign(kBlock, 0.0f);
                pointers_[channel] = buffers_[channel].data();
            }
        }

        /// @brief Zeroes every channel.
        void clear()
        {
            for (auto& buffer : buffers_)
            {
                buffer.assign(kBlock, 0.0f);
            }
        }

        /// @brief Returns the pointer table.
        float* const* pointers() { return pointers_.data(); }

        /// @brief Returns one channel.
        const std::vector<float>& channel(std::uint32_t index) const { return buffers_[index]; }

    private:
        /// @brief Stores the sixteen buffers.
        std::array<std::vector<float>, kChannels> buffers_;

        /// @brief Stores one pointer per buffer.
        std::array<float*, kChannels> pointers_;
    };

    /// @brief Returns a tap with a pass-through filter and an encode of 1 on channel 0.
    SushiDSP::room::EarlyTap unit_tap(float delay_frames, float gain)
    {
        SushiDSP::room::EarlyTap tap{};
        tap.delay_frames = delay_frames;
        tap.gain = gain;
        tap.section_a = { 1.0, 0.0, 0.0, 0.0, 0.0 };
        tap.section_b = { 1.0, 0.0, 0.0, 0.0, 0.0 };
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
        renderer.prepare(kMaxDelay, kBlock);
        return renderer;
    }

    /// @brief Returns one block that is a unit impulse at frame 0.
    std::vector<float> impulse_block()
    {
        std::vector<float> block(kBlock, 0.0f);
        block[0] = 1.0f;
        return block;
    }

    /// @brief Renders a 1 kHz sine through one set and returns channel 0 over several blocks.
    std::vector<float> render_sine(const SushiDSP::room::EarlyTapSet& set, std::uint32_t blocks)
    {
        SushiDSP::room::EarlyReflectionRenderer renderer = prepared_renderer();
        std::vector<float> input(kBlock);
        std::vector<float> output;
        output.reserve(static_cast<std::size_t>(blocks) * kBlock);
        for (std::uint32_t block = 0; block < blocks; ++block)
        {
            for (std::uint32_t frame = 0; frame < kBlock; ++frame)
            {
                const double time = static_cast<double>(block * kBlock + frame) / 48000.0;
                input[frame] = static_cast<float>(std::sin(2.0 * kPi * 1000.0 * time));
            }
            renderer.write(input.data(), kBlock);
            OutputBlock outputs;
            renderer.render(set, 0u, outputs.pointers(), kBlock);
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
                sum += static_cast<double>(late[static_cast<std::size_t>(frame)])
                    * static_cast<double>(early[static_cast<std::size_t>(frame - lag)]);
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
}

TEST(Unit_Dsp, EarlyReflectionRendererPutsAnImpulseAtTheTapsDelay)
{
    SushiDSP::room::EarlyReflectionRenderer renderer = prepared_renderer();
    const SushiDSP::room::EarlyTapSet set = single_tap_set(100.0f);
    const std::vector<float> input = impulse_block();
    renderer.write(input.data(), kBlock);
    OutputBlock outputs;
    renderer.render(set, 0u, outputs.pointers(), kBlock);
    for (std::uint32_t frame = 0; frame < kBlock; ++frame)
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

TEST(Unit_Dsp, EarlyReflectionRendererInterpolatesAFractionalDelay)
{
    SushiDSP::room::EarlyReflectionRenderer renderer = prepared_renderer();
    const SushiDSP::room::EarlyTapSet half = single_tap_set(100.5f);
    const std::vector<float> input = impulse_block();
    renderer.write(input.data(), kBlock);
    OutputBlock outputs;
    renderer.render(half, 0u, outputs.pointers(), kBlock);
    EXPECT_GT(outputs.channel(0)[100], 0.4f);
    EXPECT_GT(outputs.channel(0)[101], 0.4f);

    const std::vector<float> whole_sine = render_sine(single_tap_set(100.0f), 16u);
    const std::vector<float> half_sine = render_sine(half, 16u);
    // 2016 frames are 42 whole periods of the 1 kHz sine, so the correlation is symmetric.
    EXPECT_NEAR(lag_frames(whole_sine, half_sine, 512, 2016), 0.5, 0.01);
}

TEST(Unit_Dsp, EarlyReflectionRendererSumsOneHundredAndTwentyEightTaps)
{
    SushiDSP::room::EarlyReflectionRenderer renderer = prepared_renderer();
    SushiDSP::room::EarlyTapSet set{};
    set.count = static_cast<std::uint32_t>(SushiDSP::room::EarlyTapSet::kMaxEarlyTaps);
    for (std::uint32_t tap = 0; tap < set.count; ++tap)
    {
        set.taps[tap] = unit_tap(static_cast<float>(100u + tap), 1.0f / 128.0f);
    }
    const std::vector<float> input = impulse_block();
    renderer.write(input.data(), kBlock);
    OutputBlock outputs;
    renderer.render(set, 0u, outputs.pointers(), kBlock);
    double sum = 0.0;
    for (std::uint32_t frame = 0; frame < kBlock; ++frame)
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

TEST(Unit_Dsp, EarlyReflectionRendererEncodesIntoSixteenChannels)
{
    SushiDSP::room::EarlyReflectionRenderer renderer = prepared_renderer();
    SushiDSP::room::EarlyTapSet set = single_tap_set(100.0f);
    set.taps[0].encode[3] = 1.0f;
    const std::vector<float> input = impulse_block();
    renderer.write(input.data(), kBlock);
    OutputBlock outputs;
    renderer.render(set, 0u, outputs.pointers(), kBlock);
    for (std::uint32_t channel = 0; channel < kChannels; ++channel)
    {
        if (channel == 0u || channel == 3u)
        {
            EXPECT_EQ(outputs.channel(channel)[100], 1.0f) << "channel " << channel;
            continue;
        }
        for (std::uint32_t frame = 0; frame < kBlock; ++frame)
        {
            EXPECT_LT(std::fabs(outputs.channel(channel)[frame]), 1e-9f)
                << "channel " << channel << " frame " << frame;
        }
    }
}

TEST(Unit_Dsp, EarlyReflectionRendererKeepsTwoIndependentStateBanks)
{
    SushiDSP::room::EarlyReflectionRenderer renderer = prepared_renderer();
    SushiDSP::room::EarlyTapSet set = single_tap_set(100.0f);
    // A resonator near 1 kHz with a pole radius of 0.999 rings for many blocks.
    const double radius = 0.999;
    const double theta = 2.0 * kPi * 1000.0 / 48000.0;
    set.taps[0].section_a = { 1.0, 0.0, 0.0, -2.0 * radius * std::cos(theta), radius * radius };

    const std::vector<float> impulse = impulse_block();
    const std::vector<float> silence(kBlock, 0.0f);
    renderer.write(impulse.data(), kBlock);
    OutputBlock first_from_bank_zero;
    renderer.render(set, 0u, first_from_bank_zero.pointers(), kBlock);
    OutputBlock scratch;
    for (int block = 1; block < 10; ++block)
    {
        renderer.write(silence.data(), kBlock);
        scratch.clear();
        renderer.render(set, 0u, scratch.pointers(), kBlock);
    }

    renderer.write(impulse.data(), kBlock);
    renderer.clear_bank(1u);
    OutputBlock first_from_bank_one;
    renderer.render(set, 1u, first_from_bank_one.pointers(), kBlock);
    for (std::uint32_t frame = 0; frame < kBlock; ++frame)
    {
        EXPECT_EQ(first_from_bank_one.channel(0)[frame], first_from_bank_zero.channel(0)[frame])
            << "frame " << frame;
    }

    // Bank 0 still rings from its own history, so the same block through it is not bank 1's.
    OutputBlock ringing_bank_zero;
    renderer.render(set, 0u, ringing_bank_zero.pointers(), kBlock);
    bool differs = false;
    for (std::uint32_t frame = 0; frame < 100u; ++frame)
    {
        differs = differs || ringing_bank_zero.channel(0)[frame] != 0.0f;
    }
    EXPECT_TRUE(differs);
}

TEST(Unit_Dsp, EarlyReflectionRendererCarriesOneTapsStateBetweenBanks)
{
    // Two copies of a resonator ring in bank 0 after an impulse. Tap 0's state carried into
    // bank 1 makes bank 1 ring exactly as bank 0's tap 0 alone does; tap 1, not carried,
    // stays silent in bank 1 because the ring has gone quiet.
    SushiDSP::room::EarlyReflectionRenderer renderer = prepared_renderer();
    const double radius = 0.999;
    const double theta = 2.0 * kPi * 1000.0 / 48000.0;
    SushiDSP::room::EarlyTapSet both = single_tap_set(100.0f);
    both.taps[0].section_a = { 1.0, 0.0, 0.0, -2.0 * radius * std::cos(theta), radius * radius };
    both.count = 2u;
    both.taps[1] = both.taps[0];
    both.taps[1].delay_frames = 200.0f;
    SushiDSP::room::EarlyTapSet first_only = both;
    first_only.count = 1u;

    const std::vector<float> impulse = impulse_block();
    const std::vector<float> silence(kBlock, 0.0f);
    renderer.write(impulse.data(), kBlock);
    OutputBlock scratch;
    renderer.render(both, 0u, scratch.pointers(), kBlock);
    for (int block = 1; block < 10; ++block)
    {
        renderer.write(silence.data(), kBlock);
        scratch.clear();
        renderer.render(both, 0u, scratch.pointers(), kBlock);
    }

    renderer.carry_tap(0u, 1u, 0u);
    renderer.write(silence.data(), kBlock);
    OutputBlock from_bank_zero;
    renderer.render(first_only, 0u, from_bank_zero.pointers(), kBlock);
    OutputBlock from_bank_one;
    renderer.render(both, 1u, from_bank_one.pointers(), kBlock);
    bool rings = false;
    for (std::uint32_t frame = 0; frame < kBlock; ++frame)
    {
        EXPECT_EQ(from_bank_one.channel(0)[frame], from_bank_zero.channel(0)[frame])
            << "frame " << frame;
        rings = rings || from_bank_one.channel(0)[frame] != 0.0f;
    }
    EXPECT_TRUE(rings);
}

TEST(Unit_Dsp, EarlyReflectionRendererCarriesATapOntoItselfWithoutAChange)
{
    // The same bank as both ends of a carry is a copy onto itself: a copy of the renderer
    // that never made the call renders the same block.
    SushiDSP::room::EarlyReflectionRenderer renderer = prepared_renderer();
    const double radius = 0.999;
    const double theta = 2.0 * kPi * 1000.0 / 48000.0;
    SushiDSP::room::EarlyTapSet set = single_tap_set(100.0f);
    set.taps[0].section_a = { 1.0, 0.0, 0.0, -2.0 * radius * std::cos(theta), radius * radius };

    const std::vector<float> impulse = impulse_block();
    const std::vector<float> silence(kBlock, 0.0f);
    renderer.write(impulse.data(), kBlock);
    OutputBlock scratch;
    renderer.render(set, 0u, scratch.pointers(), kBlock);

    SushiDSP::room::EarlyReflectionRenderer untouched = renderer;
    renderer.carry_tap(0u, 0u, 0u);
    renderer.write(silence.data(), kBlock);
    untouched.write(silence.data(), kBlock);
    OutputBlock carried;
    renderer.render(set, 0u, carried.pointers(), kBlock);
    OutputBlock reference;
    untouched.render(set, 0u, reference.pointers(), kBlock);
    for (std::uint32_t frame = 0; frame < kBlock; ++frame)
    {
        EXPECT_EQ(carried.channel(0)[frame], reference.channel(0)[frame]) << "frame " << frame;
    }
}

TEST(Unit_Dsp, EarlyReflectionRendererCarriesATailAcrossBlockBoundaries)
{
    SushiDSP::room::EarlyReflectionRenderer renderer = prepared_renderer();
    const SushiDSP::room::EarlyTapSet set = single_tap_set(300.0f);
    const std::vector<float> impulse = impulse_block();
    const std::vector<float> silence(kBlock, 0.0f);

    renderer.write(impulse.data(), kBlock);
    OutputBlock first;
    renderer.render(set, 0u, first.pointers(), kBlock);
    for (std::uint32_t frame = 0; frame < kBlock; ++frame)
    {
        EXPECT_LT(std::fabs(first.channel(0)[frame]), 1e-9f) << "frame " << frame;
    }

    renderer.write(silence.data(), kBlock);
    OutputBlock second;
    renderer.render(set, 0u, second.pointers(), kBlock);
    for (std::uint32_t frame = 0; frame < kBlock; ++frame)
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
