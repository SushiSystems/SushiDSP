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

#include <array>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <new>
#include <vector>

#include <gtest/gtest.h>

#include "SushiDSP/circuits/DiodeClipperCircuit.hpp"
#include "SushiDSP/core/IAudioNode.hpp"
#include "SushiDSP/core/RigGraph.hpp"
#include "SushiDSP/core/RtSwapSlot.hpp"
#include "SushiDSP/dd3/Dd3Delay.hpp"
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
#include "SushiDSP/diezel_vh4/DiezelVh4.hpp"
#include "SushiDSP/jcm800/Jcm800.hpp"
#include "SushiDSP/mesa_mark2c/MesaMark2c.hpp"
#include "SushiDSP/peavey5150/Peavey5150.hpp"
#include "SushiDSP/tubescreamer/TubeScreamer.hpp"

#include "circuits/JcmV1aNetlist.hpp"
#include "ui/LevelProbe.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{
    std::atomic<bool> g_trap_allocations{ false };
    std::atomic<int>  g_trapped_allocation_count{ 0 };

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

    g_trapped_allocation_count.store(0, std::memory_order_relaxed);

    for (int block = 0; block < 7000; ++block)
    {
        for (std::uint16_t index = 0; index < 100; ++index)
        {
            graph.push_parameter_change(SushiDSP::ParameterChange{ 0, index, 0.5f });
        }

        g_trap_allocations.store(true, std::memory_order_relaxed);

        SushiDSP::ProcessContext context{ output_channels, nullptr, 256, 0.0 };
        graph.process(context);

        g_trap_allocations.store(false, std::memory_order_relaxed);
    }

    EXPECT_EQ(g_trapped_allocation_count.load(std::memory_order_relaxed), 0);
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

    g_trapped_allocation_count.store(0, std::memory_order_relaxed);
    g_trap_allocations.store(true, std::memory_order_relaxed);

    for (int block = 0; block < 1000; ++block)
    {
        fft.forward(time_domain.data(), real_bins.data(), imag_bins.data());
        fft.inverse(real_bins.data(), imag_bins.data(), reconstructed.data());
    }

    g_trap_allocations.store(false, std::memory_order_relaxed);

    EXPECT_EQ(g_trapped_allocation_count.load(std::memory_order_relaxed), 0);
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

    g_trapped_allocation_count.store(0, std::memory_order_relaxed);
    g_trap_allocations.store(true, std::memory_order_relaxed);

    for (int block = 0; block < 500; ++block)
    {
        const float* oversampled = oversampler.upsample(input.data(), block_size);
        oversampler.downsample(oversampled, block_size * 8, output.data());
    }

    g_trap_allocations.store(false, std::memory_order_relaxed);

    EXPECT_EQ(g_trapped_allocation_count.load(std::memory_order_relaxed), 0);
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

    g_trapped_allocation_count.store(0, std::memory_order_relaxed);
    g_trap_allocations.store(true, std::memory_order_relaxed);

    for (int block = 0; block < 1000; ++block)
    {
        convolution.process(input.data(), output.data());
    }

    g_trap_allocations.store(false, std::memory_order_relaxed);

    EXPECT_EQ(g_trapped_allocation_count.load(std::memory_order_relaxed), 0);
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

    g_trapped_allocation_count.store(0, std::memory_order_relaxed);
    g_trap_allocations.store(true, std::memory_order_relaxed);

    for (int block = 0; block < 200; ++block)
    {
        convolution.process(input.data(), output.data(), frames);
    }

    g_trap_allocations.store(false, std::memory_order_relaxed);

    EXPECT_EQ(g_trapped_allocation_count.load(std::memory_order_relaxed), 0);
}

TEST(Unit_Dsp, DiodeClipperCircuitProcessPerformsNoHeapAllocation)
{
    SushiDSP::DiodeClipperCircuit circuit(1000.0f, 1e-12f, 0.02585f);

    float output = 0.0f;

    g_trapped_allocation_count.store(0, std::memory_order_relaxed);
    g_trap_allocations.store(true, std::memory_order_relaxed);

    for (int i = 0; i < 2000; ++i)
    {
        const float source_voltage = static_cast<float>((i % 21) - 10) * 0.05f;
        output = circuit.process_sample(source_voltage);
    }

    g_trap_allocations.store(false, std::memory_order_relaxed);

    EXPECT_EQ(g_trapped_allocation_count.load(std::memory_order_relaxed), 0);
    EXPECT_TRUE(std::isfinite(output));
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

    g_trapped_allocation_count.store(0, std::memory_order_relaxed);
    g_trap_allocations.store(true, std::memory_order_relaxed);

    for (int block = 0; block < 1000; ++block)
    {
        SushiDSP::ProcessContext context{ output_channels, input_channels, 256, 0.0 };
        node.process(context);
    }

    g_trap_allocations.store(false, std::memory_order_relaxed);

    EXPECT_EQ(g_trapped_allocation_count.load(std::memory_order_relaxed), 0);
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

    g_trapped_allocation_count.store(0, std::memory_order_relaxed);
    g_trap_allocations.store(true, std::memory_order_relaxed);
    for (int block = 0; block < 100; ++block)
    {
        node.process(context);
    }
    g_trap_allocations.store(false, std::memory_order_relaxed);

    EXPECT_EQ(g_trapped_allocation_count.load(std::memory_order_relaxed), 0);
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

    g_trapped_allocation_count.store(0, std::memory_order_relaxed);
    g_trap_allocations.store(true, std::memory_order_relaxed);
    for (int block = 0; block < 100; ++block)
    {
        node.process(context);
    }
    g_trap_allocations.store(false, std::memory_order_relaxed);

    EXPECT_EQ(g_trapped_allocation_count.load(std::memory_order_relaxed), 0);
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

    g_trapped_allocation_count.store(0, std::memory_order_relaxed);
    g_trap_allocations.store(true, std::memory_order_relaxed);

    for (int block = 0; block < 500; ++block)
    {
        SushiDSP::ProcessContext context{ output_channels, input_channels, 256, 0.0 };
        node.process(context);
    }

    g_trap_allocations.store(false, std::memory_order_relaxed);

    EXPECT_EQ(g_trapped_allocation_count.load(std::memory_order_relaxed), 0);
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

    g_trapped_allocation_count.store(0, std::memory_order_relaxed);
    g_trap_allocations.store(true, std::memory_order_relaxed);

    for (int block = 0; block < 500; ++block)
    {
        SushiDSP::ProcessContext context{ output_channels, input_channels, 256, 0.0 };
        node.process(context);
    }

    g_trap_allocations.store(false, std::memory_order_relaxed);

    EXPECT_EQ(g_trapped_allocation_count.load(std::memory_order_relaxed), 0);
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

    g_trapped_allocation_count.store(0, std::memory_order_relaxed);
    g_trap_allocations.store(true, std::memory_order_relaxed);

    for (int block = 0; block < 500; ++block)
    {
        SushiDSP::ProcessContext context{ output_channels, input_channels, 256, 0.0 };
        node.process(context);
    }

    g_trap_allocations.store(false, std::memory_order_relaxed);

    EXPECT_EQ(g_trapped_allocation_count.load(std::memory_order_relaxed), 0);
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

    g_trapped_allocation_count.store(0, std::memory_order_relaxed);
    g_trap_allocations.store(true, std::memory_order_relaxed);

    for (int block = 0; block < 500; ++block)
    {
        SushiDSP::ProcessContext context{ output_channels, input_channels, 256, 0.0 };
        node.process(context);
    }

    g_trap_allocations.store(false, std::memory_order_relaxed);

    EXPECT_EQ(g_trapped_allocation_count.load(std::memory_order_relaxed), 0);
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

    g_trapped_allocation_count.store(0, std::memory_order_relaxed);
    g_trap_allocations.store(true, std::memory_order_relaxed);

    for (int block = 0; block < 500; ++block)
    {
        SushiDSP::ProcessContext context{ output_channels, input_channels, 256, 0.0 };
        node.process(context);
    }

    g_trap_allocations.store(false, std::memory_order_relaxed);

    EXPECT_EQ(g_trapped_allocation_count.load(std::memory_order_relaxed), 0);
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

    g_trapped_allocation_count.store(0, std::memory_order_relaxed);
    g_trap_allocations.store(true, std::memory_order_relaxed);

    for (int block = 0; block < 500; ++block)
    {
        SushiDSP::ProcessContext context{ output_channels, input_channels, 256, 0.0 };
        node.process(context);
    }

    g_trap_allocations.store(false, std::memory_order_relaxed);

    EXPECT_EQ(g_trapped_allocation_count.load(std::memory_order_relaxed), 0);
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

    g_trapped_allocation_count.store(0, std::memory_order_relaxed);
    g_trap_allocations.store(true, std::memory_order_relaxed);

    for (int block = 0; block < 500; ++block)
    {
        SushiDSP::ProcessContext context{ output_channels, input_channels, 256, 0.0 };
        node.process(context);
    }

    g_trap_allocations.store(false, std::memory_order_relaxed);

    EXPECT_EQ(g_trapped_allocation_count.load(std::memory_order_relaxed), 0);
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

    g_trapped_allocation_count.store(0, std::memory_order_relaxed);
    g_trap_allocations.store(true, std::memory_order_relaxed);

    for (int block = 0; block < 500; ++block)
    {
        SushiDSP::ProcessContext context{ output_channels, input_channels, 256, 0.0 };
        node.process(context);
    }

    g_trap_allocations.store(false, std::memory_order_relaxed);

    EXPECT_EQ(g_trapped_allocation_count.load(std::memory_order_relaxed), 0);
}

TEST(Unit_Dsp, AsioSampleConverterPerformsNoHeapAllocation)
{
    std::uint8_t bytes[4] = { 0, 0, 0, 0 };

    g_trapped_allocation_count.store(0, std::memory_order_relaxed);
    g_trap_allocations.store(true, std::memory_order_relaxed);

    for (int i = 0; i < 10000; ++i)
    {
        const float decoded = SushiDSP::asio_sample_to_float(SushiDSP::AsioSampleFormat::Int32LSB, bytes);
        SushiDSP::float_to_asio_sample(SushiDSP::AsioSampleFormat::Int32LSB, decoded, bytes);
    }

    g_trap_allocations.store(false, std::memory_order_relaxed);

    EXPECT_EQ(g_trapped_allocation_count.load(std::memory_order_relaxed), 0);
}

TEST(Unit_Dsp, RtSwapSlotAcquireForAudioThreadPerformsNoHeapAllocation)
{
    SushiDSP::RtSwapSlot<int> slot;
    slot.publish(std::make_unique<int>(3));

    g_trapped_allocation_count.store(0, std::memory_order_relaxed);
    g_trap_allocations.store(true, std::memory_order_relaxed);

    int accumulator = 0;
    for (int i = 0; i < 10000; ++i)
    {
        const int* object = slot.acquire_for_audio_thread();
        accumulator += (object != nullptr && slot.has_active()) ? *object : 0;
    }

    g_trap_allocations.store(false, std::memory_order_relaxed);

    EXPECT_EQ(accumulator, 30000);
    EXPECT_EQ(g_trapped_allocation_count.load(std::memory_order_relaxed), 0);
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

    g_trapped_allocation_count.store(0, std::memory_order_relaxed);

    for (int block = 0; block < 4; ++block)
    {
        impulse_response.samples[0] = 1.0f - 0.1f * static_cast<float>(block);
        ASSERT_TRUE(node.set_cabinet(impulse_response));
        ASSERT_TRUE(node.has_cabinet());

        g_trap_allocations.store(true, std::memory_order_relaxed);
        SushiDSP::ProcessContext context{ output_channels, input_channels, 256, 0.0 };
        node.process(context);
        g_trap_allocations.store(false, std::memory_order_relaxed);
    }

    EXPECT_EQ(g_trapped_allocation_count.load(std::memory_order_relaxed), 0);

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

    g_trapped_allocation_count.store(0, std::memory_order_relaxed);
    g_trap_allocations.store(true, std::memory_order_relaxed);
    for (int i = 0; i < 64; ++i)
    {
        probe.report(block.data(), 256);
    }
    g_trap_allocations.store(false, std::memory_order_relaxed);

    EXPECT_EQ(g_trapped_allocation_count.load(std::memory_order_relaxed), 0);
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

    g_trapped_allocation_count.store(0, std::memory_order_relaxed);
    g_trap_allocations.store(true, std::memory_order_relaxed);

    for (int sample = 0; sample < 256; ++sample)
    {
        const double drive = 0.2
            * std::sin(2.0 * M_PI * 220.0 * static_cast<double>(sample) / 48000.0);
        const std::array<double, 2> inputs{ drive, kSupplyVolts };
        circuit.process_sample(port, inputs, outputs);
    }

    g_trap_allocations.store(false, std::memory_order_relaxed);

    EXPECT_EQ(g_trapped_allocation_count.load(std::memory_order_relaxed), 0);
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

    g_trapped_allocation_count.store(0, std::memory_order_relaxed);

    int rebuilt_blocks = 0;
    for (int block = 0; block < 16; ++block)
    {
        const double position = 0.1 + 0.05 * static_cast<double>(block);

        g_trap_allocations.store(true, std::memory_order_relaxed);

        circuit.set_potentiometer(pot_index, position);
        rebuilt_blocks += circuit.update_matrices_if_dirty() ? 1 : 0;
        for (int sample = 0; sample < 64; ++sample)
        {
            circuit.process_sample(port, silence, outputs);
        }

        g_trap_allocations.store(false, std::memory_order_relaxed);
    }

    EXPECT_EQ(rebuilt_blocks, 16);
    EXPECT_EQ(g_trapped_allocation_count.load(std::memory_order_relaxed), 0);
}

TEST(Unit_Dsp, DampedNewtonSolverPerformsNoHeapAllocation)
{
    SushiDSP::dk::DampedNewtonSolver<2> solver;
    ExponentialPairSystem               system;
    std::array<double, 2>               v{ 0.0, 0.0 };

    g_trapped_allocation_count.store(0, std::memory_order_relaxed);
    g_trap_allocations.store(true, std::memory_order_relaxed);

    int converged = 0;
    for (int solve = 0; solve < 256; ++solve)
    {
        v[0] = 2.0;
        v[1] = -1.0;
        converged += solver.solve(system, v) ? 1 : 0;
    }

    g_trap_allocations.store(false, std::memory_order_relaxed);

    EXPECT_EQ(converged, 256);
    EXPECT_NEAR(v[0], 0.0, 1e-9);
    EXPECT_NEAR(v[1], 0.0, 1e-9);
    EXPECT_EQ(g_trapped_allocation_count.load(std::memory_order_relaxed), 0);
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

    g_trapped_allocation_count.store(0, std::memory_order_relaxed);
    g_trap_allocations.store(true, std::memory_order_relaxed);

    for (int block = 0; block < 200; ++block)
    {
        resampler.process(input.data(), 256u, output.data(),
                          static_cast<std::uint32_t>(output.size()));
    }

    g_trap_allocations.store(false, std::memory_order_relaxed);

    EXPECT_EQ(g_trapped_allocation_count.load(std::memory_order_relaxed), 0);
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

    g_trapped_allocation_count.store(0, std::memory_order_relaxed);
    g_trap_allocations.store(true, std::memory_order_relaxed);

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

    g_trap_allocations.store(false, std::memory_order_relaxed);

    EXPECT_EQ(g_trapped_allocation_count.load(std::memory_order_relaxed), 0);
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

    g_trapped_allocation_count.store(0, std::memory_order_relaxed);
    g_trap_allocations.store(true, std::memory_order_relaxed);

    node.process(context);
    node.set_parameter(mode_index, long_mode_value);
    node.process(context);
    node.set_mode(SushiDSP::dd3::Dd3Mode::Short);
    node.process(context);

    g_trap_allocations.store(false, std::memory_order_relaxed);

    EXPECT_EQ(g_trapped_allocation_count.load(std::memory_order_relaxed), 0);
    for (std::size_t i = 0; i < output.size(); ++i)
    {
        EXPECT_TRUE(std::isfinite(output[i]));
    }
}
