/**************************************************************************/
/* test_rt_safety.cpp                                                     */
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

// _USE_MATH_DEFINES must precede every include: it is what makes MSVC's <cmath> declare M_PI,
// and <cmath> may otherwise be pulled in transitively before this point.
#define _USE_MATH_DEFINES

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <new>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "SushiDSP/core/IAudioNode.hpp"
#include "SushiDSP/core/RigGraph.hpp"
#include "SushiDSP/core/RtSwapSlot.hpp"
#include "SushiDSP/dd3/Dd3Delay.hpp"
#include "SushiDSP/capture/Metronome.hpp"
#include "SushiDSP/capture/TrackRecorder.hpp"
#include "SushiDSP/capture/TrackPlayer.hpp"
#include "SushiDSP/capture/TrackTimeline.hpp"
#include "SushiDSP/capture/VariRateReader.hpp"
#include "SushiDSP/device/AsioSampleConverter.hpp"
#include "SushiDSP/io/Ir.hpp"
#include "SushiDSP/math/Fft.hpp"
#include "SushiDSP/math/Oversampler.hpp"
#include "SushiDSP/math/PartitionedConvolution.hpp"
#include "SushiDSP/math/Resampler.hpp"
#include "SushiDSP/math/StreamingConvolution.hpp"
#include "SushiDSP/math/dk/DampedNewtonSolver.hpp"
#include "SushiDSP/math/dk/DkCircuit.hpp"
#include "SushiDSP/math/dk/DkNetlist.hpp"
#include "SushiDSP/routing/SplitterNode.hpp"
#include "SushiDSP/routing/MixerNode.hpp"
#include "SushiDSP/routing/PanNode.hpp"
#include "SushiDSP/diezel_vh4/DiezelVh4.hpp"
#include "SushiDSP/jcm800/Jcm800.hpp"
#include "SushiDSP/mesa_mark2c/MesaMark2c.hpp"
#include "SushiDSP/peavey5150/Peavey5150.hpp"
#include "SushiDSP/room/CrossoverPair.hpp"
#include "SushiDSP/room/EarlyReflectionRenderer.hpp"
#include "SushiDSP/room/EarlyTapSet.hpp"
#include "SushiDSP/room/FdnDesign.hpp"
#include "SushiDSP/room/LateReverb.hpp"
#include "SushiDSP/room/ModalBank.hpp"
#include "SushiDSP/room/ModalDesign.hpp"
#include "SushiDSP/room/RoomNode.hpp"
#include "SushiDSP/tubescreamer/TubeScreamer.hpp"

#include "RoomFixtures.hpp"
#include "circuits/JcmV1aNetlist.hpp"
#include "ui/LevelProbe.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{
    // Each thread carries its own trap, so a case only ever counts the thread that opened its window.
    thread_local std::atomic<bool> g_trap_allocations{ false };
    thread_local std::atomic<int>  g_trapped_allocation_count{ 0 };

    /// @brief Counts the calling thread's heap allocations over the window a case opens, and
    /// closes that window on the way out however the case leaves it.
    class AllocationTrap
    {
    public:
        /// @brief Zeroes the calling thread's count and leaves the trap closed.
        AllocationTrap() noexcept
        {
            g_trapped_allocation_count.store(0, std::memory_order_relaxed);
        }

        /// @brief Closes the window, so a case that returns early cannot poison a later one.
        ~AllocationTrap() { disarm(); }

        AllocationTrap(const AllocationTrap&) = delete;
        AllocationTrap& operator=(const AllocationTrap&) = delete;

        /// @brief Opens the window, counting every allocation this thread makes from here.
        void arm() noexcept { g_trap_allocations.store(true, std::memory_order_relaxed); }

        /// @brief Closes the window.
        void disarm() noexcept { g_trap_allocations.store(false, std::memory_order_relaxed); }

        /// @brief Reports how many allocations this thread made inside the window.
        int count() const noexcept
        {
            return g_trapped_allocation_count.load(std::memory_order_relaxed);
        }
    };

    class SumNode : public SushiDSP::IAudioNode
    {
    public:
        void prepare(const SushiDSP::PrepareInfo&) override {}

        void process(SushiDSP::ProcessContext& context) override
        {
            for (std::uint32_t frame = 0; frame < context.frames; ++frame)
            {
                context.outputs[0][frame] = 1.0f;
            }
        }

        SushiDSP::PortLayout ports() const override { return { 0, 1 }; }

        void reset() override {}

        float get_parameter(const char*) const override { return 0.0f; }
        void set_parameter(const char*, float) override {}
    };
}

void* operator new(std::size_t size)
{
    if (g_trap_allocations.load(std::memory_order_relaxed))
    {
        g_trapped_allocation_count.fetch_add(1, std::memory_order_relaxed);
    }
    void* ptr = std::malloc(size != 0 ? size : 1);
    if (ptr == nullptr)
    {
        throw std::bad_alloc();
    }
    return ptr;
}

void* operator new[](std::size_t size)
{
    return ::operator new(size);
}

void operator delete(void* ptr) noexcept
{
    std::free(ptr);
}

void operator delete[](void* ptr) noexcept
{
    std::free(ptr);
}

void operator delete(void* ptr, std::size_t) noexcept
{
    std::free(ptr);
}

void operator delete[](void* ptr, std::size_t) noexcept
{
    std::free(ptr);
}

TEST(Unit_Dsp, RigGraphProcessPerformsNoHeapAllocationAcrossManyBlocks)
{
    SushiDSP::RigGraph graph(256, 4);
    graph.add_node(std::make_unique<SumNode>());

    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    graph.compile(info);

    std::vector<float> output_buffer(256, 0.0f);
    float* output_channels[1] = { output_buffer.data() };

    AllocationTrap trap;

    for (int block = 0; block < 7000; ++block)
    {
        for (std::uint16_t index = 0; index < 100; ++index)
        {
            graph.push_parameter_change(SushiDSP::ParameterChange{ 0, index, 0.5f });
        }

        trap.arm();

        SushiDSP::ProcessContext context{ output_channels, nullptr, 256, 0.0 };
        graph.process(context);

        trap.disarm();
    }

    EXPECT_EQ(trap.count(), 0);
}

namespace
{
    class StereoSourceNode : public SushiDSP::IAudioNode
    {
    public:
        void prepare(const SushiDSP::PrepareInfo&) override {}

        void process(SushiDSP::ProcessContext& context) override
        {
            for (std::uint32_t frame = 0; frame < context.frames; ++frame)
            {
                context.outputs[0][frame] = 1.0f;
                context.outputs[1][frame] = -1.0f;
            }
        }

        SushiDSP::PortLayout ports() const override { return { 0, 2 }; }

        void reset() override {}

        float get_parameter(const char*) const override { return 0.0f; }
        void set_parameter(const char*, float) override {}
    };

    class MonoPassThroughNode : public SushiDSP::IAudioNode
    {
    public:
        void prepare(const SushiDSP::PrepareInfo&) override {}

        void process(SushiDSP::ProcessContext& context) override
        {
            for (std::uint32_t frame = 0; frame < context.frames; ++frame)
            {
                context.outputs[0][frame] = context.inputs[0] != nullptr ? context.inputs[0][frame] : 0.0f;
            }
        }

        SushiDSP::PortLayout ports() const override { return { 1, 1 }; }

        void reset() override {}

        float get_parameter(const char*) const override { return 0.0f; }
        void set_parameter(const char*, float) override {}
    };
}

TEST(Unit_Dsp, RigGraphStereoProcessPerformsNoHeapAllocation)
{
    // Exercises every arithmetic path of the edge adaptation: a stereo producer summed into a
    // mono consumer, a fan-in summed per channel, and a mono sink widened to a stereo host.
    SushiDSP::RigGraph graph(256, 4);
    const auto first_source  = graph.add_node(std::make_unique<StereoSourceNode>());
    const auto second_source = graph.add_node(std::make_unique<StereoSourceNode>());
    const auto sink          = graph.add_node(std::make_unique<MonoPassThroughNode>());
    graph.connect(first_source, sink);
    graph.connect(second_source, sink);

    SushiDSP::PrepareInfo info{ 48000.0, 256, 2 };
    graph.compile(info);

    std::vector<float> left(256, 0.0f);
    std::vector<float> right(256, 0.0f);
    float* output_channels[2] = { left.data(), right.data() };

    AllocationTrap trap;

    for (int block = 0; block < 2000; ++block)
    {
        trap.arm();

        SushiDSP::ProcessContext context{ output_channels, nullptr, 256, 0.0 };
        graph.process(context);

        trap.disarm();
    }

    EXPECT_EQ(trap.count(), 0);
}

TEST(Unit_Dsp, FftForwardAndInverseRoundTripPerformsNoHeapAllocation)
{
    const std::uint32_t size = 256;
    SushiDSP::Fft       fft(size);

    std::vector<float> time_domain(size, 0.0f);
    std::vector<float> real_bins(size, 0.0f);
    std::vector<float> imag_bins(size, 0.0f);
    std::vector<float> reconstructed(size, 0.0f);
    for (std::uint32_t i = 0; i < size; ++i)
    {
        time_domain[i] = static_cast<float>(i % 7) - 3.0f;
    }

    AllocationTrap trap;
    trap.arm();

    for (int block = 0; block < 1000; ++block)
    {
        fft.forward(time_domain.data(), real_bins.data(), imag_bins.data());
        fft.inverse(real_bins.data(), imag_bins.data(), reconstructed.data());
    }

    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
    for (std::uint32_t i = 0; i < size; ++i)
    {
        EXPECT_NEAR(reconstructed[i], time_domain[i], 1e-3f);
    }
}

TEST(Unit_Dsp, Oversampler8xRoundTripPerformsNoHeapAllocation)
{
    const std::uint32_t block_size = 64;
    SushiDSP::Oversampler8x oversampler(block_size);

    std::vector<float> input(block_size, 0.0f);
    std::vector<float> output(block_size, 0.0f);
    for (std::uint32_t i = 0; i < block_size; ++i)
    {
        input[i] = static_cast<float>(i % 5) * 0.1f - 0.2f;
    }

    AllocationTrap trap;
    trap.arm();

    for (int block = 0; block < 500; ++block)
    {
        const float* oversampled = oversampler.upsample(input.data(), block_size);
        oversampler.downsample(oversampled, block_size * 8, output.data());
    }

    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
}

TEST(Unit_Dsp, PartitionedConvolutionProcessPerformsNoHeapAllocation)
{
    const std::uint32_t partition_size = 64;

    std::vector<float> impulse_response(160, 0.0f);
    for (std::size_t i = 0; i < impulse_response.size(); ++i)
    {
        impulse_response[i] = 1.0f / (1.0f + static_cast<float>(i));
    }

    SushiDSP::PartitionedConvolution convolution(
        partition_size, impulse_response.data(), static_cast<std::uint32_t>(impulse_response.size()));

    std::vector<float> input(partition_size, 0.0f);
    std::vector<float> output(partition_size, 0.0f);
    for (std::uint32_t i = 0; i < partition_size; ++i)
    {
        input[i] = static_cast<float>(i % 5) * 0.1f - 0.2f;
    }

    AllocationTrap trap;
    trap.arm();

    for (int block = 0; block < 1000; ++block)
    {
        convolution.process(input.data(), output.data());
    }

    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
}

TEST(Unit_Dsp, StreamingConvolutionProcessPerformsNoHeapAllocation)
{
    const std::uint32_t partition_size = 128;

    std::vector<float> impulse_response(300, 0.0f);
    for (std::size_t i = 0; i < impulse_response.size(); ++i)
    {
        impulse_response[i] = 1.0f / (1.0f + static_cast<float>(i));
    }

    SushiDSP::StreamingConvolution convolution(
        partition_size, impulse_response.data(), static_cast<std::uint32_t>(impulse_response.size()), 4096);

    const std::uint32_t frames = 37;
    std::vector<float> input(frames, 0.0f);
    std::vector<float> output(frames, 0.0f);
    for (std::uint32_t i = 0; i < frames; ++i)
    {
        input[i] = static_cast<float>(i % 5) * 0.1f - 0.2f;
    }

    AllocationTrap trap;
    trap.arm();

    for (int block = 0; block < 200; ++block)
    {
        convolution.process(input.data(), output.data(), frames);
    }

    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
}

TEST(Unit_Dsp, TubeScreamerProcessPerformsNoHeapAllocation)
{
    SushiDSP::TubeScreamer node;
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    node.prepare(info);
    node.set_drive(0.6f);
    node.set_tone(0.4f);
    node.set_level(0.9f);

    std::vector<float> input(256);
    for (std::size_t i = 0; i < input.size(); ++i)
    {
        input[i] = 0.3f * std::sin(2.0f * static_cast<float>(M_PI) * 220.0f * i / 48000.0f);
    }
    std::vector<float> output(256, 0.0f);
    const float* input_channels[1] = { input.data() };
    float* output_channels[1] = { output.data() };

    AllocationTrap trap;
    trap.arm();

    // Moving a knob before every block is the path the plain loop misses: a wiper that has
    // changed marks the block dirty and update_matrices_if_dirty() re-derives inside process(),
    // which is where a solver would allocate if it were going to. This is the D6c instance of
    // the trap D6b's DkCircuitPotentiometerUpdatePerformsNoHeapAllocation established, now on a
    // shipping node rather than on a bare circuit. Both knobs sweep their full travel so every
    // derivation sees a genuinely new matrix.
    for (int block = 0; block < 1000; ++block)
    {
        const float position = static_cast<float>(block) / 999.0f;
        node.set_drive(position);
        node.set_tone(1.0f - position);
        SushiDSP::ProcessContext context{ output_channels, input_channels, 256, 0.0 };
        node.process(context);
    }

    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
}

TEST(Unit_Dsp, SplitterNodeProcessPerformsNoHeapAllocation)
{
    SushiDSP::SplitterNode node;
    node.prepare(SushiDSP::PrepareInfo{ 48000.0, 256, 1 });

    std::vector<float> input(256, 1.0f);
    std::vector<float> output(256, 0.0f);
    const float* inputs[1] = { input.data() };
    float* outputs[1] = { output.data() };
    SushiDSP::ProcessContext context{ outputs, inputs, 256, 0.0 };

    AllocationTrap trap;
    trap.arm();
    for (int block = 0; block < 100; ++block)
    {
        node.process(context);
    }
    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
}

TEST(Unit_Dsp, MixerNodeProcessPerformsNoHeapAllocation)
{
    SushiDSP::MixerNode node;
    node.prepare(SushiDSP::PrepareInfo{ 48000.0, 256, 1 });

    std::vector<float> a(256, 1.0f);
    std::vector<float> b(256, 1.0f);
    std::vector<float> output(256, 0.0f);
    const float* inputs[SushiDSP::MixerNode::kMaxFanIn] = { a.data(), b.data(), nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
    float* outputs[1] = { output.data() };
    SushiDSP::ProcessContext context{ outputs, inputs, 256, 0.0 };

    AllocationTrap trap;
    trap.arm();
    for (int block = 0; block < 100; ++block)
    {
        node.process(context);
    }
    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
}

TEST(Unit_Dsp, PanNodeProcessPerformsNoHeapAllocation)
{
    // apply_pan()'s cos/sin recompute runs on stack values only: no heap, lock, syscall, throw.
    SushiDSP::PanNode node;
    node.prepare(SushiDSP::PrepareInfo{ 48000.0, 256, 1 });
    node.set_parameter("pan", 0.5f);

    std::vector<float> input(256, 1.0f);
    std::vector<float> out_left(256, 0.0f);
    std::vector<float> out_right(256, 0.0f);
    const float* inputs[1] = { input.data() };
    float* outputs[2] = { out_left.data(), out_right.data() };
    SushiDSP::ProcessContext context{ outputs, inputs, 256, 0.0 };

    AllocationTrap trap;
    trap.arm();
    for (int block = 0; block < 100; ++block)
    {
        node.process(context);
    }
    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
}

TEST(Unit_Dsp, Jcm800ProcessPerformsNoHeapAllocationWithoutCabinet)
{
    // This is also the trap for Jcm800::process()'s per-block set_high_shelf() call: recomputing
    // the presence shelf's RBJ coefficients every block looks expensive, but it is pure double
    // arithmetic (pow/sin/cos/sqrt) with no heap, lock, syscall, or throw behind it, so it must
    // not register here.
    SushiDSP::Jcm800 node;
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    node.prepare(info);
    node.set_preamp(0.8f);
    node.set_master(0.0f);
    node.set_bass(0.6f);
    node.set_middle(0.4f);
    node.set_treble(0.7f);
    node.set_presence(0.9f);

    std::vector<float> input(256);
    for (std::size_t i = 0; i < input.size(); ++i)
    {
        input[i] = 0.3f * std::sin(2.0f * static_cast<float>(M_PI) * 220.0f * i / 48000.0f);
    }
    std::vector<float> output(256, 0.0f);
    const float* input_channels[1] = { input.data() };
    float* output_channels[1] = { output.data() };

    AllocationTrap trap;
    trap.arm();

    for (int block = 0; block < 500; ++block)
    {
        SushiDSP::ProcessContext context{ output_channels, input_channels, 256, 0.0 };
        node.process(context);
    }

    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
}

TEST(Unit_Dsp, Peavey5150ProcessPerformsNoHeapAllocationWithoutCabinet)
{
    SushiDSP::Peavey5150 node;
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    node.prepare(info);
    node.set_pre_gain(0.8f);
    node.set_low(0.6f);
    node.set_mid(0.4f);
    node.set_high(0.7f);
    node.set_post_gain(0.0f);
    node.set_resonance(0.5f);
    node.set_presence(0.9f);

    std::vector<float> input(256);
    for (std::size_t i = 0; i < input.size(); ++i)
    {
        input[i] = 0.3f * std::sin(2.0f * static_cast<float>(M_PI) * 220.0f * i / 48000.0f);
    }
    std::vector<float> output(256, 0.0f);
    const float* input_channels[1] = { input.data() };
    float* output_channels[1] = { output.data() };

    AllocationTrap trap;
    trap.arm();

    for (int block = 0; block < 500; ++block)
    {
        SushiDSP::ProcessContext context{ output_channels, input_channels, 256, 0.0 };
        node.process(context);
    }

    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
}

TEST(Unit_Dsp, MesaMark2cProcessPerformsNoHeapAllocationWithoutCabinet)
{
    SushiDSP::MesaMark2c node;
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    node.prepare(info);
    node.set_volume_1(0.8f);
    node.set_treble(0.7f);
    node.set_bass(0.6f);
    node.set_middle(0.4f);
    node.set_lead_drive(0.5f);
    node.set_lead_master(0.5f);
    node.set_master_1(0.0f);
    node.set_presence(0.9f);
    node.set_equalizer_band(0, 0.2f);
    node.set_equalizer_band(1, -0.2f);
    node.set_equalizer_band(2, 0.1f);
    node.set_equalizer_band(3, -0.1f);
    node.set_equalizer_band(4, 0.0f);

    std::vector<float> input(256);
    for (std::size_t i = 0; i < input.size(); ++i)
    {
        input[i] = 0.3f * std::sin(2.0f * static_cast<float>(M_PI) * 220.0f * i / 48000.0f);
    }
    std::vector<float> output(256, 0.0f);
    const float* input_channels[1] = { input.data() };
    float* output_channels[1] = { output.data() };

    AllocationTrap trap;
    trap.arm();

    for (int block = 0; block < 500; ++block)
    {
        SushiDSP::ProcessContext context{ output_channels, input_channels, 256, 0.0 };
        node.process(context);
    }

    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
}

TEST(Unit_Dsp, DiezelVh4ProcessPerformsNoHeapAllocationWithoutCabinet)
{
    SushiDSP::DiezelVh4 node;
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    node.prepare(info);
    node.set_gain(0.8f);
    node.set_bass(0.6f);
    node.set_middle(0.4f);
    node.set_treble(0.7f);
    node.set_volume(0.5f);
    node.set_deep(0.5f);
    node.set_presence(0.9f);
    node.set_master(0.0f);

    std::vector<float> input(256);
    for (std::size_t i = 0; i < input.size(); ++i)
    {
        input[i] = 0.3f * std::sin(2.0f * static_cast<float>(M_PI) * 220.0f * i / 48000.0f);
    }
    std::vector<float> output(256, 0.0f);
    const float* input_channels[1] = { input.data() };
    float* output_channels[1] = { output.data() };

    AllocationTrap trap;
    trap.arm();

    for (int block = 0; block < 500; ++block)
    {
        SushiDSP::ProcessContext context{ output_channels, input_channels, 256, 0.0 };
        node.process(context);
    }

    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
}

TEST(Unit_Dsp, Jcm800ProcessPerformsNoHeapAllocationWithCabinet)
{
    // The cabinet path is a separate allocation-risk surface from the no-cabinet path: it runs
    // PartitionedConvolution's FFT, spectrum history, and overlap-add buffers, all of which must
    // have been sized in prepare(). 500 blocks is more than the impulse response's partition
    // count, so the circular spectrum history wraps at least once under the trap.
    std::vector<float> impulse_response(200, 0.0f);
    impulse_response[0] = 1.0f;
    for (std::size_t i = 1; i < impulse_response.size(); ++i)
    {
        impulse_response[i] = 0.5f * impulse_response[i - 1];
    }

    SushiDSP::Jcm800 node(impulse_response);
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    node.prepare(info);
    node.set_preamp(0.6f);
    node.set_master(0.0f);

    std::vector<float> input(256);
    for (std::size_t i = 0; i < input.size(); ++i)
    {
        input[i] = 0.2f * std::sin(2.0f * static_cast<float>(M_PI) * 220.0f * i / 48000.0f);
    }
    std::vector<float> output(256, 0.0f);
    const float* input_channels[1] = { input.data() };
    float* output_channels[1] = { output.data() };

    AllocationTrap trap;
    trap.arm();

    for (int block = 0; block < 500; ++block)
    {
        SushiDSP::ProcessContext context{ output_channels, input_channels, 256, 0.0 };
        node.process(context);
    }

    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
}

TEST(Unit_Dsp, Peavey5150ProcessPerformsNoHeapAllocationWithCabinet)
{
    // Same separate surface as Jcm800ProcessPerformsNoHeapAllocationWithCabinet: the cabinet path
    // runs PartitionedConvolution's FFT, spectrum history and overlap-add buffers, all sized in
    // prepare(). 500 blocks wraps the circular spectrum history at least once under the trap.
    std::vector<float> impulse_response(200, 0.0f);
    impulse_response[0] = 1.0f;
    for (std::size_t i = 1; i < impulse_response.size(); ++i)
    {
        impulse_response[i] = 0.5f * impulse_response[i - 1];
    }

    SushiDSP::Peavey5150 node(impulse_response);
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    node.prepare(info);
    node.set_pre_gain(0.8f);
    node.set_low(0.6f);
    node.set_mid(0.4f);
    node.set_high(0.7f);
    node.set_post_gain(0.0f);
    node.set_resonance(0.5f);
    node.set_presence(0.9f);

    std::vector<float> input(256);
    for (std::size_t i = 0; i < input.size(); ++i)
    {
        input[i] = 0.2f * std::sin(2.0f * static_cast<float>(M_PI) * 220.0f * i / 48000.0f);
    }
    std::vector<float> output(256, 0.0f);
    const float* input_channels[1] = { input.data() };
    float* output_channels[1] = { output.data() };

    AllocationTrap trap;
    trap.arm();

    for (int block = 0; block < 500; ++block)
    {
        SushiDSP::ProcessContext context{ output_channels, input_channels, 256, 0.0 };
        node.process(context);
    }

    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
}

TEST(Unit_Dsp, MesaMark2cProcessPerformsNoHeapAllocationWithCabinet)
{
    // Same separate surface as Jcm800ProcessPerformsNoHeapAllocationWithCabinet: the cabinet path
    // runs PartitionedConvolution's FFT, spectrum history and overlap-add buffers, all sized in
    // prepare(). 500 blocks wraps the circular spectrum history at least once under the trap.
    std::vector<float> impulse_response(200, 0.0f);
    impulse_response[0] = 1.0f;
    for (std::size_t i = 1; i < impulse_response.size(); ++i)
    {
        impulse_response[i] = 0.5f * impulse_response[i - 1];
    }

    SushiDSP::MesaMark2c node(impulse_response);
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    node.prepare(info);
    node.set_volume_1(0.8f);
    node.set_treble(0.7f);
    node.set_bass(0.6f);
    node.set_middle(0.4f);
    node.set_lead_drive(0.5f);
    node.set_lead_master(0.5f);
    node.set_master_1(0.0f);
    node.set_presence(0.9f);
    node.set_equalizer_band(0, 0.2f);
    node.set_equalizer_band(1, -0.2f);
    node.set_equalizer_band(2, 0.1f);
    node.set_equalizer_band(3, -0.1f);
    node.set_equalizer_band(4, 0.0f);

    std::vector<float> input(256);
    for (std::size_t i = 0; i < input.size(); ++i)
    {
        input[i] = 0.2f * std::sin(2.0f * static_cast<float>(M_PI) * 220.0f * i / 48000.0f);
    }
    std::vector<float> output(256, 0.0f);
    const float* input_channels[1] = { input.data() };
    float* output_channels[1] = { output.data() };

    AllocationTrap trap;
    trap.arm();

    for (int block = 0; block < 500; ++block)
    {
        SushiDSP::ProcessContext context{ output_channels, input_channels, 256, 0.0 };
        node.process(context);
    }

    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
}

TEST(Unit_Dsp, DiezelVh4ProcessPerformsNoHeapAllocationWithCabinet)
{
    // Same separate surface as Jcm800ProcessPerformsNoHeapAllocationWithCabinet: the cabinet path
    // runs PartitionedConvolution's FFT, spectrum history and overlap-add buffers, all sized in
    // prepare(). 500 blocks wraps the circular spectrum history at least once under the trap.
    std::vector<float> impulse_response(200, 0.0f);
    impulse_response[0] = 1.0f;
    for (std::size_t i = 1; i < impulse_response.size(); ++i)
    {
        impulse_response[i] = 0.5f * impulse_response[i - 1];
    }

    SushiDSP::DiezelVh4 node(impulse_response);
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    node.prepare(info);
    node.set_gain(0.8f);
    node.set_bass(0.6f);
    node.set_middle(0.4f);
    node.set_treble(0.7f);
    node.set_volume(0.5f);
    node.set_deep(0.5f);
    node.set_presence(0.9f);
    node.set_master(0.0f);

    std::vector<float> input(256);
    for (std::size_t i = 0; i < input.size(); ++i)
    {
        input[i] = 0.2f * std::sin(2.0f * static_cast<float>(M_PI) * 220.0f * i / 48000.0f);
    }
    std::vector<float> output(256, 0.0f);
    const float* input_channels[1] = { input.data() };
    float* output_channels[1] = { output.data() };

    AllocationTrap trap;
    trap.arm();

    for (int block = 0; block < 500; ++block)
    {
        SushiDSP::ProcessContext context{ output_channels, input_channels, 256, 0.0 };
        node.process(context);
    }

    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
}

TEST(Unit_Dsp, AsioSampleConverterPerformsNoHeapAllocation)
{
    std::uint8_t bytes[4] = { 0, 0, 0, 0 };

    AllocationTrap trap;
    trap.arm();

    for (int i = 0; i < 10000; ++i)
    {
        const float decoded = SushiDSP::asio_sample_to_float(SushiDSP::AsioSampleFormat::Int32LSB, bytes);
        SushiDSP::float_to_asio_sample(SushiDSP::AsioSampleFormat::Int32LSB, decoded, bytes);
    }

    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
}

TEST(Unit_Dsp, RtSwapSlotAcquireForAudioThreadPerformsNoHeapAllocation)
{
    SushiDSP::RtSwapSlot<int> slot;
    slot.publish(std::make_unique<int>(3));

    AllocationTrap trap;
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

TEST(Unit_Dsp, Jcm800SetCabinetSwapsWithoutAllocatingOnTheAudioThread)
{
    // A cabinet published from the control thread must reach process() through RtSwapSlot's
    // single atomic acquire - no allocation, no lock. Only the audio thread's half of a swap has
    // to be RT-safe (publish() builds the convolver and is expected to allocate), so the trap is
    // armed around the processing blocks and disarmed across each set_cabinet(). That way every
    // trapped block is one that picks up a freshly published cabinet, which is the case the name
    // promises - a version of this test that published everything up front would only be proving
    // that convolving allocates nothing.
    SushiDSP::Jcm800 node;
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    node.prepare(info);

    SushiDSP::Ir impulse_response;
    impulse_response.sample_rate = 48000.0;
    impulse_response.samples.assign(2048, 0.0f);
    impulse_response.samples[0] = 1.0f;

    std::vector<float> input(256);
    for (std::size_t i = 0; i < input.size(); ++i)
    {
        input[i] = 0.2f * std::sin(2.0f * static_cast<float>(M_PI) * 220.0f * i / 48000.0f);
    }
    std::vector<float> output(256, 0.0f);
    const float* input_channels[1] = { input.data() };
    float* output_channels[1] = { output.data() };

    AllocationTrap trap;

    for (int block = 0; block < 4; ++block)
    {
        impulse_response.samples[0] = 1.0f - 0.1f * static_cast<float>(block);
        ASSERT_TRUE(node.set_cabinet(impulse_response));
        ASSERT_TRUE(node.has_cabinet());

        trap.arm();
        SushiDSP::ProcessContext context{ output_channels, input_channels, 256, 0.0 };
        node.process(context);
        trap.disarm();
    }

    EXPECT_EQ(trap.count(), 0);

    node.reclaim_cabinets();
}

namespace
{
    // The netlist, its component constants and the triode glue live in
    // tests/circuits/JcmV1aNetlist.hpp, shared with the D6b stage test and the benchmark.
    using namespace SushiDSP::test_circuits;

    using DkStageCircuit = SushiDSP::dk::DkCircuit<6, 2, 2, 1>;
    using DkStageNetlist = SushiDSP::dk::DkNetlist<6, 2, 2, 1>;

    /// @brief A two-variable nonlinear system with a known root, for the solver trap.
    ///
    /// The pair (exp(v0) - 1 + v1/2, v0/2 + exp(v1) - 1) has its root at v0 = v1 = 0, where its
    /// Jacobian is [[1, 1/2], [1/2, 1]] and so comfortably non-singular. The exponentials make
    /// the line search do real work from a distant start rather than converging in one step.
    struct ExponentialPairSystem
    {
        /// @brief Evaluates the residual and its Jacobian at one iterate.
        /// @param v The iterate.
        /// @param residual Receives the residual.
        /// @param jacobian Receives the 2x2 Jacobian.
        void residual_and_jacobian(const std::array<double, 2>&              v,
                                   std::array<double, 2>&                    residual,
                                   std::array<std::array<double, 2>, 2>&     jacobian) const
        {
            const double first = std::exp(v[0]);
            const double second = std::exp(v[1]);
            residual[0] = first - 1.0 + 0.5 * v[1];
            residual[1] = 0.5 * v[0] + second - 1.0;
            jacobian[0][0] = first;
            jacobian[0][1] = 0.5;
            jacobian[1][0] = 0.5;
            jacobian[1][1] = second;
        }
    };
}

TEST(Unit_Dsp, LevelProbeReportPerformsNoHeapAllocation)
{
    ui::LevelProbe probe;
    std::vector<float> block(256, 0.25f);

    AllocationTrap trap;
    trap.arm();
    for (int i = 0; i < 64; ++i)
    {
        probe.report(block.data(), 256);
    }
    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
}

TEST(Unit_Dsp, DkCircuitProcessPerformsNoHeapAllocation)
{
    DkStageNetlist netlist;
    build_jcm_v1a(netlist);

    DkStageCircuit circuit;
    TriodeStagePort   port;
    ASSERT_TRUE(circuit.prepare(netlist, 48000.0));

    const std::array<double, 2> silence{ 0.0, kSupplyVolts };
    std::array<double, 1>       outputs{};
    for (int sample = 0; sample < 4800; ++sample)
    {
        circuit.process_sample(port, silence, outputs);
    }

    AllocationTrap trap;
    trap.arm();

    for (int sample = 0; sample < 256; ++sample)
    {
        const double drive = 0.2
            * std::sin(2.0 * M_PI * 220.0 * static_cast<double>(sample) / 48000.0);
        const std::array<double, 2> inputs{ drive, kSupplyVolts };
        circuit.process_sample(port, inputs, outputs);
    }

    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
    EXPECT_EQ(circuit.nan_fallback_samples(), 0u);
}

TEST(Unit_Dsp, DkCircuitPotentiometerUpdatePerformsNoHeapAllocation)
{
    // The one audio-thread path that re-runs a derivation (plan Ruling 3): a pot move invalidates
    // the DK matrices and update_matrices_if_dirty() rebuilds them at the top of the next block.
    // If anything in that rebuild allocated, Ruling 3 would have to be replaced by a control-
    // thread rebuild plus an RtSwapSlot hand-off, so this case is the one that decides it.
    DkStageNetlist netlist;
    const std::size_t pot_index = build_jcm_v1a(netlist);

    DkStageCircuit circuit;
    TriodeStagePort   port;
    ASSERT_TRUE(circuit.prepare(netlist, 48000.0));

    const std::array<double, 2> silence{ 0.0, kSupplyVolts };
    std::array<double, 1>       outputs{};

    AllocationTrap trap;

    int rebuilt_blocks = 0;
    for (int block = 0; block < 16; ++block)
    {
        const double position = 0.1 + 0.05 * static_cast<double>(block);

        trap.arm();

        circuit.set_potentiometer(pot_index, position);
        rebuilt_blocks += circuit.update_matrices_if_dirty() ? 1 : 0;
        for (int sample = 0; sample < 64; ++sample)
        {
            circuit.process_sample(port, silence, outputs);
        }

        trap.disarm();
    }

    EXPECT_EQ(rebuilt_blocks, 16);
    EXPECT_EQ(trap.count(), 0);
}

TEST(Unit_Dsp, DampedNewtonSolverPerformsNoHeapAllocation)
{
    SushiDSP::dk::DampedNewtonSolver<2> solver;
    ExponentialPairSystem               system;
    std::array<double, 2>               v{ 0.0, 0.0 };

    AllocationTrap trap;
    trap.arm();

    int converged = 0;
    for (int solve = 0; solve < 256; ++solve)
    {
        v[0] = 2.0;
        v[1] = -1.0;
        converged += solver.solve(system, v) ? 1 : 0;
    }

    trap.disarm();

    EXPECT_EQ(converged, 256);
    EXPECT_NEAR(v[0], 0.0, 1e-9);
    EXPECT_NEAR(v[1], 0.0, 1e-9);
    EXPECT_EQ(trap.count(), 0);
}

TEST(Unit_Dsp, ResamplerProcessPerformsNoHeapAllocation)
{
    // The DD-3's decimating leg at the VCO's slowest clock, which is where the Resampler is
    // widest: the prototype's half width of 34 (input samples; true crossings are fewer at
    // fc = 0.45) stretches to 34 / (20000 / 48000) = 81.6, so interpolate() runs two loops of 82
    // and touches 164 taps per output, and prepare() sizes the history ring for exactly that.
    SushiDSP::Resampler resampler;
    resampler.prepare(48000.0, 20000.0, 256u, 20000.0 / 48000.0);

    std::vector<float> input(256);
    for (std::size_t i = 0; i < input.size(); ++i)
    {
        input[i] = 0.3f * std::sin(2.0f * static_cast<float>(M_PI) * 440.0f * i / 48000.0f);
    }
    std::vector<float> output(resampler.maximum_output_frames(256u) + 16u, 0.0f);

    AllocationTrap trap;
    trap.arm();

    for (int block = 0; block < 200; ++block)
    {
        resampler.process(input.data(), 256u, output.data(),
                          static_cast<std::uint32_t>(output.size()));
    }

    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
}

TEST(Unit_Dsp, ResamplerRateChangePerformsNoHeapAllocation)
{
    // The case plan Ruling 3 rests on. The DD-3 re-clocks every 8 frames, so one 256-frame
    // block calls set_rates 32 times; a ratio change that rebuilt a filter bank would put the
    // node off the audio thread entirely. The sweep runs the VCO's whole 20-82.5 kHz range, so
    // the ratio reaches both ends of the clamp prepare() was given.
    SushiDSP::Resampler resampler;
    resampler.prepare(48000.0, 82500.0, 8u, 20000.0 / 48000.0);

    std::vector<float> input(8);
    for (std::size_t i = 0; i < input.size(); ++i)
    {
        input[i] = 0.25f * std::sin(2.0f * static_cast<float>(M_PI) * 440.0f * i / 48000.0f);
    }
    std::array<float, 64> output{};

    AllocationTrap trap;
    trap.arm();

    for (int block = 0; block < 200; ++block)
    {
        for (int sub_block = 0; sub_block < 32; ++sub_block)
        {
            const double clock_hz = 20000.0
                + (82500.0 - 20000.0) * static_cast<double>(sub_block) / 31.0;
            resampler.set_rates(48000.0, clock_hz);
            resampler.process(input.data(), 8u, output.data(),
                              static_cast<std::uint32_t>(output.size()));
        }
    }

    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
}

TEST(Unit_Dsp, Dd3DelayProcessPerformsNoHeapAllocation)
{
    // Two things happen inside this trap window that no other node does. process() drives both
    // Resamplers' set_rates 32 times per block through the 8-frame clock grid, and a MODE
    // change clears the ring (plan Ruling 14) - once through set_parameter by index, which only
    // records the mode so that process() applies it at the next block start, and once through
    // set_mode, which applies it on the spot. Middle holds 4096 samples and Long 16384, so both
    // changes below move N and both pay the std::fill.
    SushiDSP::Dd3Delay node;
    node.prepare(SushiDSP::PrepareInfo{ 48000.0, 256, 1 });
    node.set_time(0.4f);
    node.set_feedback(0.45f);
    node.set_level(0.5f);

    std::vector<float> input(256);
    for (std::size_t i = 0; i < input.size(); ++i)
    {
        input[i] = 0.3f * std::sin(2.0f * static_cast<float>(M_PI) * 220.0f * i / 48000.0f);
    }
    std::vector<float> output(256, 0.0f);
    const float* input_channels[1] = { input.data() };
    float* output_channels[1] = { output.data() };
    SushiDSP::ProcessContext context{ output_channels, input_channels, 256, 0.0 };

    const std::uint16_t mode_index = 3u;
    const float         long_mode_value =
        static_cast<float>(static_cast<int>(SushiDSP::dd3::Dd3Mode::Long));

    AllocationTrap trap;
    trap.arm();

    node.process(context);
    node.set_parameter(mode_index, long_mode_value);
    node.process(context);
    node.set_mode(SushiDSP::dd3::Dd3Mode::Short);
    node.process(context);

    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
    for (std::size_t i = 0; i < output.size(); ++i)
    {
        EXPECT_TRUE(std::isfinite(output[i]));
    }
}

TEST(Unit_Dsp, TrackRecorderWritePerformsNoHeapAllocation)
{
    // The one capture path the audio thread touches. arm() allocates the rings and starts the
    // writer thread before the trap opens, and the first block is written and drained outside it
    // so that WavWriter's stream buffer already exists: what the trap then sees is the ring copy
    // and, once the ring fills, the drop counter.
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "sushidsp_rt_safety_recorder";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    SushiDSP::TrackRecorder recorder(2u, 48000.0, 65536);
    ASSERT_TRUE(recorder.arm((root / "rt_safety.wav").string()));

    std::vector<float> left(256, 0.25f);
    std::vector<float> right(256, -0.25f);
    const float* channels[2] = { left.data(), right.data() };

    ASSERT_TRUE(recorder.write(channels, 256u));
    const std::chrono::steady_clock::time_point deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (recorder.frames_written() < 256u && std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_GE(recorder.frames_written(), 256u);

    AllocationTrap trap;
    trap.arm();

    for (int block = 0; block < 200; ++block)
    {
        recorder.write(channels, 256u);
    }

    trap.disarm();

    EXPECT_EQ(trap.count(), 0);

    recorder.stop();
    std::filesystem::remove_all(root);
}

TEST(Unit_Dsp, TrackPlayerProcessPerformsNoHeapAllocation)
{
    // Every timeline is published and every converter sized before the trap window opens, so
    // what is measured is the mix itself: eight slot acquisitions, eight lanes of sixteen clips
    // walked, the pan trigonometry, a loop range that splits each block into two runs, and both
    // the unity copy path and the converted one.
    SushiDSP::TrackPlayer player;
    SushiDSP::Transport   transport;

    for (std::size_t track = 0; track < SushiDSP::TrackPlayer::kMaxTracks; ++track)
    {
        auto   take = std::make_unique<SushiDSP::Take>(track % 2 == 0 ? 1u : 2u, 48000.0, 4096u);
        float* samples = take->mutable_channel(0);
        for (std::size_t frame = 0; frame < 4096; ++frame)
        {
            samples[frame] = 0.25f * std::sin(2.0f * static_cast<float>(M_PI) * 110.0f
                                              * static_cast<float>(frame) / 48000.0f);
        }

        const SushiDSP::Take* borrowed =
            player.takes().insert(std::unique_ptr<const SushiDSP::Take>(std::move(take)));

        std::vector<SushiDSP::Clip> clips;
        for (std::size_t clip = 0; clip < SushiDSP::TrackTimeline::kMaxClips; ++clip)
        {
            clips.push_back(SushiDSP::Clip{ borrowed, clip * 200u, 64u, 150u });
        }

        player.set_timeline(track, std::unique_ptr<SushiDSP::TrackTimeline>(
                                       new SushiDSP::TrackTimeline(player.takes(), clips)));
        player.set_pan(track, -1.0f + 0.25f * static_cast<float>(track));
        player.set_gain(track, 0.5f);
    }
    player.set_mute(1, true);
    player.prepare(256u);
    transport.set_loop(0, 3000);

    std::vector<float> left(256, 0.0f);
    std::vector<float> right(256, 0.0f);
    float*             outputs[2] = { left.data(), right.data() };

    AllocationTrap trap;
    trap.arm();

    for (double speed : { 1.0, 0.5 })
    {
        transport.set_speed(speed);
        for (int block = 0; block < 200; ++block)
        {
            std::fill(left.begin(), left.end(), 0.0f);
            std::fill(right.begin(), right.end(), 0.0f);
            player.process(outputs, 2u, 256u, transport);
            transport.advance(256);
        }
    }

    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
    for (std::size_t frame = 0; frame < left.size(); ++frame)
    {
        EXPECT_TRUE(std::isfinite(left[frame]));
        EXPECT_TRUE(std::isfinite(right[frame]));
    }
}

TEST(Unit_Dsp, TrackPlayerTimelineSwapPerformsNoHeapAllocationOnTheAudioThread)
{
    // Every replacement timeline is built before the run, and the window is opened only around
    // process(), so what is measured is the audio thread picking up a lane that changed under
    // it: one acquire of a new generation and a converter that has to restart on a clip it was
    // not inside a block ago.
    SushiDSP::TrackPlayer player;
    SushiDSP::Transport   transport;

    auto   take = std::make_unique<SushiDSP::Take>(2u, 48000.0, 4096u);
    float* samples = take->mutable_channel(0);
    for (std::size_t frame = 0; frame < 4096; ++frame)
    {
        samples[frame] = 0.25f * std::sin(2.0f * static_cast<float>(M_PI) * 110.0f
                                          * static_cast<float>(frame) / 48000.0f);
    }
    const SushiDSP::Take* borrowed =
        player.takes().insert(std::unique_ptr<const SushiDSP::Take>(std::move(take)));

    constexpr int kSwaps = 64;
    std::vector<std::unique_ptr<SushiDSP::TrackTimeline>> prepared;
    for (int swap = 0; swap < kSwaps; ++swap)
    {
        std::vector<SushiDSP::Clip> clips;
        for (std::size_t clip = 0; clip < SushiDSP::TrackTimeline::kMaxClips; ++clip)
        {
            clips.push_back(SushiDSP::Clip{ borrowed,
                                            static_cast<std::uint64_t>(swap) + clip * 300u, 32u,
                                            250u });
        }
        prepared.push_back(std::unique_ptr<SushiDSP::TrackTimeline>(
            new SushiDSP::TrackTimeline(player.takes(), clips)));
    }

    player.prepare(256u);
    player.set_pan(0, -0.5f);
    transport.set_speed(0.75);

    std::vector<float> left(256, 0.0f);
    std::vector<float> right(256, 0.0f);
    float*             outputs[2] = { left.data(), right.data() };

    AllocationTrap trap;

    for (int swap = 0; swap < kSwaps; ++swap)
    {
        player.set_timeline(0, std::move(prepared[static_cast<std::size_t>(swap)]));

        trap.arm();
        std::fill(left.begin(), left.end(), 0.0f);
        std::fill(right.begin(), right.end(), 0.0f);
        player.process(outputs, 2u, 256u, transport);
        trap.disarm();

        transport.advance(256);
    }

    EXPECT_EQ(trap.count(), 0);
    for (std::size_t frame = 0; frame < left.size(); ++frame)
    {
        EXPECT_TRUE(std::isfinite(left[frame]));
        EXPECT_TRUE(std::isfinite(right[frame]));
    }
}

TEST(Unit_Dsp, VariRateReaderReadPerformsNoHeapAllocation)
{
    // The converters are sized before the trap window opens, so what is measured is the read
    // itself: a set_rates() on every speed change and one push per take frame consumed.
    SushiDSP::Take take(2u, 48000.0, 48000u);
    for (std::uint32_t channel = 0; channel < 2u; ++channel)
    {
        float* samples = take.mutable_channel(channel);
        for (std::size_t frame = 0; frame < 48000u; ++frame)
        {
            samples[frame] = 0.25f * std::sin(2.0f * static_cast<float>(M_PI) * 110.0f
                                              * static_cast<float>(frame) / 48000.0f);
        }
    }

    SushiDSP::VariRateReader reader;
    reader.prepare(2u, 256u);

    std::vector<float> left(256, 0.0f);
    std::vector<float> right(256, 0.0f);
    float*             outputs[2] = { left.data(), right.data() };

    AllocationTrap trap;
    trap.arm();

    for (double speed : { 0.5, 1.0, 2.0 })
    {
        reader.reset();
        std::uint64_t cursor = 0u;
        for (int block = 0; block < 200; ++block)
        {
            cursor = reader.read(take, cursor, speed, outputs, 2u, 256u);
        }
    }

    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
    for (std::size_t frame = 0; frame < left.size(); ++frame)
    {
        EXPECT_TRUE(std::isfinite(left[frame]));
        EXPECT_TRUE(std::isfinite(right[frame]));
    }
}

TEST(Unit_Dsp, MetronomeProcessPerformsNoHeapAllocation)
{
    // The bursts are built before the trap window opens, so what is measured is the block: two
    // atomic loads, a beat index and a copy out of a vector already sized.
    SushiDSP::MusicalClock clock;
    clock.set_tempo(137.4);

    SushiDSP::Metronome metronome(48000.0);
    metronome.set_enabled(true);
    metronome.set_level(0.5f);

    SushiDSP::Transport transport;
    transport.play();

    std::vector<float> left(256, 0.0f);
    std::vector<float> right(256, 0.0f);
    float*             outputs[2] = { left.data(), right.data() };

    AllocationTrap trap;
    trap.arm();

    for (int block = 0; block < 400; ++block)
    {
        std::fill(left.begin(), left.end(), 0.0f);
        std::fill(right.begin(), right.end(), 0.0f);
        metronome.process(outputs, 2u, 256u, transport, clock);
        transport.advance(256u);
    }

    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
    for (std::size_t frame = 0; frame < left.size(); ++frame)
    {
        EXPECT_TRUE(std::isfinite(left[frame]));
        EXPECT_TRUE(std::isfinite(right[frame]));
    }
}

namespace
{
    /// @brief Returns the garage with an omni microphone 2.5 m in front of the cabinet.
    SushiDSP::room::RoomScene room_trap_scene(double microphone_x)
    {
        SushiDSP::room::RoomScene scene = SushiDSP::test::garage_scene();
        scene.source_position = { 1.5, 2.0, 0.5 };
        scene.microphone_position = { microphone_x, 2.0, 1.2 };
        scene.microphone_pattern = SushiDSP::room::PolarPattern::Omni;
        return scene;
    }

    /// @brief Fills a block with a 220 Hz tone at 0.3 of full scale.
    void fill_room_tone(std::vector<float>& block)
    {
        for (std::size_t i = 0; i < block.size(); ++i)
        {
            block[i] = 0.3f * std::sin(2.0f * static_cast<float>(M_PI) * 220.0f
                                       * static_cast<float>(i) / 48000.0f);
        }
    }
}

TEST(Unit_Dsp, RoomNodeProcessPerformsNoHeapAllocation)
{
    // prepare() builds the spherical head and publishes the first set outside the trap; the
    // first trapped block is the one that adopts it, a copy into the node's own bank.
    SushiDSP::room::RoomNode node;
    node.prepare(SushiDSP::PrepareInfo{ 48000.0, 256, 1 });
    std::string reason;
    ASSERT_TRUE(node.set_scene(room_trap_scene(4.0), reason)) << reason;

    std::vector<float> input(256);
    fill_room_tone(input);
    std::vector<float> left(256, 0.0f);
    std::vector<float> right(256, 0.0f);
    const float* input_channels[1] = { input.data() };
    float* output_channels[2] = { left.data(), right.data() };
    SushiDSP::ProcessContext context{ output_channels, input_channels, 256, 0.0 };

    AllocationTrap trap;
    trap.arm();
    for (int block = 0; block < 16; ++block)
    {
        node.set_parameter(std::uint16_t{ 0 }, block % 2 == 0 ? 0.7f : 1.0f);
        node.set_parameter(std::uint16_t{ 1 }, -3.0f);
        node.process(context);
    }
    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
    for (std::size_t i = 0; i < left.size(); ++i)
    {
        EXPECT_TRUE(std::isfinite(left[i]));
        EXPECT_TRUE(std::isfinite(right[i]));
    }
}

TEST(Unit_Dsp, RoomNodeProcessPerformsNoHeapAllocationAcrossAPublish)
{
    // Only the audio thread's half of a publish is trapped: set_scene solves and allocates on
    // the control thread, so the trap closes around it and opens around the blocks that pick
    // the new set up, start the crossfade and run it to its end. The node renders the
    // microphone, and the four publishes walk the modal bank through each of its paths: a
    // moved microphone ramps the gains read at it, a moved listener changes nothing the
    // microphone hears, a changed dimension blends the spare bank in from zero.
    SushiDSP::room::RoomNode node;
    node.set_parameter("listener", 1.0f);
    node.prepare(SushiDSP::PrepareInfo{ 48000.0, 256, 1 });

    std::vector<float> input(256);
    fill_room_tone(input);
    std::vector<float> left(256, 0.0f);
    std::vector<float> right(256, 0.0f);
    const float* input_channels[1] = { input.data() };
    float* output_channels[2] = { left.data(), right.data() };
    SushiDSP::ProcessContext context{ output_channels, input_channels, 256, 0.0 };

    AllocationTrap trap;
    for (int publish = 0; publish < 4; ++publish)
    {
        SushiDSP::room::RoomScene scene = room_trap_scene(3.0 + 0.25 * publish);
        scene.listener_position.y = 2.0 + 0.1 * (publish % 2);
        scene.dimensions.x = publish < 2 ? 6.0 : 6.2;
        std::string reason;
        ASSERT_TRUE(node.set_scene(scene, reason)) << reason;

        trap.arm();
        for (int block = 0; block < 3; ++block)
        {
            node.process(context);
        }
        trap.disarm();

        node.reclaim_rooms();
    }

    EXPECT_EQ(trap.count(), 0);
    for (std::size_t i = 0; i < left.size(); ++i)
    {
        EXPECT_TRUE(std::isfinite(left[i]));
    }
}

TEST(Unit_Dsp, RoomNodeProcessPerformsNoHeapAllocationForEitherListener)
{
    // The listener parameter's id path publishes on the control thread, outside the trap;
    // the blocks that adopt each set and fade it in are trapped, first for the ears, then for
    // the microphone, and the index path inside the trap stores the choice and nothing else.
    SushiDSP::room::RoomNode node;
    node.prepare(SushiDSP::PrepareInfo{ 48000.0, 256, 1 });
    std::string reason;
    ASSERT_TRUE(node.set_scene(room_trap_scene(4.0), reason)) << reason;

    std::vector<float> input(256);
    fill_room_tone(input);
    std::vector<float> left(256, 0.0f);
    std::vector<float> right(256, 0.0f);
    const float* input_channels[1] = { input.data() };
    float* output_channels[2] = { left.data(), right.data() };
    SushiDSP::ProcessContext context{ output_channels, input_channels, 256, 0.0 };

    AllocationTrap trap;
    for (const float listener : { 0.0f, 1.0f, 0.0f })
    {
        node.set_parameter("listener", listener);
        trap.arm();
        for (int block = 0; block < 3; ++block)
        {
            node.set_parameter(std::uint16_t{ 2 }, 1.0f - listener);
            node.process(context);
        }
        trap.disarm();
        node.reclaim_rooms();
    }

    EXPECT_EQ(trap.count(), 0);
    for (std::size_t i = 0; i < left.size(); ++i)
    {
        EXPECT_TRUE(std::isfinite(left[i]));
        EXPECT_TRUE(std::isfinite(right[i]));
    }
}

TEST(Unit_Dsp, EarlyReflectionRendererRenderPerformsNoHeapAllocation)
{
    SushiDSP::room::EarlyReflectionRenderer renderer;
    renderer.prepare(8397, 256);

    SushiDSP::room::EarlyTapSet set{};
    set.count = SushiDSP::room::EarlyTapSet::kMaxEarlyTaps;
    for (std::uint32_t tap = 0; tap < set.count; ++tap)
    {
        set.taps[tap].delay_frames = 1.5f + 60.0f * static_cast<float>(tap);
        set.taps[tap].gain = 1.0f / (1.0f + static_cast<float>(tap));
        set.taps[tap].section_a = { 0.5, 0.25, 0.0, -0.25, 0.0 };
        set.taps[tap].section_b = { 1.0, 0.0, 0.0, 0.0, 0.0 };
        set.taps[tap].encode.fill(0.1f);
    }

    std::vector<float> input(256);
    fill_room_tone(input);
    std::array<std::vector<float>, SushiDSP::room::EarlyReflectionRenderer::kOutputChannels>
        buffers;
    std::array<float*, SushiDSP::room::EarlyReflectionRenderer::kOutputChannels> outputs{};
    for (std::size_t channel = 0; channel < buffers.size(); ++channel)
    {
        buffers[channel].assign(256, 0.0f);
        outputs[channel] = buffers[channel].data();
    }

    AllocationTrap trap;
    trap.arm();
    for (int block = 0; block < 8; ++block)
    {
        renderer.write(input.data(), 256);
        renderer.render(set, 0, outputs.data(), 256);
        renderer.render(set, 1, outputs.data(), 256);
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

TEST(Unit_Dsp, ModalBankProcessPerformsNoHeapAllocation)
{
    // Every one of kMaxModes resonators runs, at the garage's 68.6 Hz mode with a 1 s decay:
    // r = 10^(-3 / 48000) = 0.99985610, a1 = -2 r cos(2 pi 68.6 / 48000), a2 = r^2.
    SushiDSP::room::ModalBank bank;
    bank.prepare(256);

    auto design = std::make_unique<SushiDSP::room::ModalDesign>();
    const double radius = std::pow(10.0, -3.0 / 48000.0);
    const double theta = 2.0 * M_PI * 68.6 / 48000.0;
    design->count = static_cast<std::uint32_t>(SushiDSP::room::ModalDesign::kMaxModes);
    for (std::uint32_t mode = 0; mode < design->count; ++mode)
    {
        design->modes[mode].section = { 1e-3, 0.0, -1e-3, -2.0 * radius * std::cos(theta),
                                        radius * radius };
        design->modes[mode].source_gain = 0.5f;
        design->modes[mode].ear_gain = { 0.7f, -0.7f };
    }
    design->crossover_hz = 278.18f;
    design->level_scale = 1.0f;

    std::vector<float> input(256);
    fill_room_tone(input);
    std::vector<float> left(256, 0.0f);
    std::vector<float> right(256, 0.0f);

    AllocationTrap trap;
    trap.arm();
    for (int block = 0; block < 8; ++block)
    {
        bank.process(*design, input.data(), left.data(), right.data(), 256);
        bank.process(*design, input.data(), left.data(), right.data(), 1);
    }
    bank.reset();
    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
    for (std::size_t i = 0; i < left.size(); ++i)
    {
        EXPECT_TRUE(std::isfinite(left[i]));
        EXPECT_TRUE(std::isfinite(right[i]));
    }
}

TEST(Unit_Dsp, CrossoverPairProcessPerformsNoHeapAllocation)
{
    // The pair is placed, run, moved and bypassed inside the trap, as the node does at every
    // adopt; set_crossover is arithmetic on the audio thread and must stay so.
    SushiDSP::room::CrossoverPair pair;
    std::vector<float> input(256);
    fill_room_tone(input);
    std::vector<float> low(256, 0.0f);
    std::vector<float> high(256, 0.0f);

    AllocationTrap trap;
    trap.arm();
    pair.set_crossover(48000.0, 278.18);
    for (int block = 0; block < 8; ++block)
    {
        if (block == 4)
        {
            pair.set_crossover(48000.0, 356.1);
        }
        if (block == 6)
        {
            pair.set_crossover(48000.0, 0.0);
        }
        for (std::size_t i = 0; i < input.size(); ++i)
        {
            pair.process_sample(input[i], low[i], high[i]);
        }
    }
    pair.reset();
    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
    for (std::size_t i = 0; i < low.size(); ++i)
    {
        EXPECT_TRUE(std::isfinite(low[i]));
        EXPECT_TRUE(std::isfinite(high[i]));
    }
}

TEST(Unit_Dsp, LateReverbProcessPerformsNoHeapAllocation)
{
    // The second design moves every line by 37 frames, so the trapped blocks include the
    // 512-frame length crossfade as well as the steady network.
    SushiDSP::room::LateReverb reverb;
    reverb.prepare(25200);

    SushiDSP::room::FdnDesign design{};
    const SushiDSP::room::FdnDesign::Section unity = { 1.0, 0.0, 0.0, 0.0, 0.0 };
    const SushiDSP::room::FdnDesign::Section damped = { 0.9, 0.0, 0.0, 0.0, 0.0 };
    for (std::size_t line = 0; line < SushiDSP::room::FdnDesign::kLineCount; ++line)
    {
        design.line_frames[line] = static_cast<std::uint32_t>(1000 + 97 * line);
        design.line_section_a[line] = damped;
        design.line_section_b[line] = unity;
    }
    design.tone_correction = { unity, unity };
    design.coherence_a = { unity, unity };
    design.coherence_b = { unity, unity };
    design.input_gain = 0.5f;
    design.delay_to_crossover_frames = 1749.0f;

    SushiDSP::room::FdnDesign moved = design;
    for (std::uint32_t& frames : moved.line_frames)
    {
        frames += 37u;
    }

    std::vector<float> input(256);
    fill_room_tone(input);
    std::vector<float> left(256, 0.0f);
    std::vector<float> right(256, 0.0f);

    AllocationTrap trap;
    trap.arm();
    for (int block = 0; block < 8; ++block)
    {
        const SushiDSP::room::FdnDesign& current = block < 4 ? design : moved;
        reverb.process(current, input.data(), left.data(), right.data(), 256);
        reverb.process(current, input.data(), left.data(), left.data(), 256);
    }
    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
    for (std::size_t i = 0; i < left.size(); ++i)
    {
        EXPECT_TRUE(std::isfinite(left[i]));
        EXPECT_TRUE(std::isfinite(right[i]));
    }
}
