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
#include "SushiDSP/math/Fft.hpp"
#include "SushiDSP/math/Oversampler.hpp"
#include "SushiDSP/math/PartitionedConvolution.hpp"
#include "SushiDSP/jcm800/Jcm800.hpp"
#include "SushiDSP/tubescreamer/TubeScreamer.hpp"

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
    g_trap_allocations.store(true, std::memory_order_relaxed);

    for (int block = 0; block < 7000; ++block)
    {
        SushiDSP::ProcessContext context{ output_channels, nullptr, 256, 0.0 };
        graph.process(context);
    }

    g_trap_allocations.store(false, std::memory_order_relaxed);

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
    node.set_master(0.7f);
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
    node.set_master(0.7f);

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
