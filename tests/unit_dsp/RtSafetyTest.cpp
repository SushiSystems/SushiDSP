// RtSafetyTest.cpp
//
// Copyright 2026 SushiDSP Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <atomic>
#include <cstdlib>
#include <memory>
#include <new>
#include <vector>

#include <gtest/gtest.h>

#include "SushiDSP/core/IAudioNode.hpp"
#include "SushiDSP/core/RigGraph.hpp"
#include "SushiDSP/math/Fft.hpp"

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

TEST(UnitDsp, RigGraphProcessPerformsNoHeapAllocationAcrossManyBlocks)
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

TEST(UnitDsp, FftForwardAndInverseRoundTripPerformsNoHeapAllocation)
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
