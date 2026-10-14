/**************************************************************************/
/* test_rt_safety.cpp                                                     */
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
 * @file test_rt_safety.cpp
 * @brief Tests that the audio-thread paths perform no heap allocation.
 * @author Mustafa Garip
 */

// _USE_MATH_DEFINES precedes every include so that MSVC's <cmath> declares M_PI.
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

#include <SushiDSP/foundation/core/audio_node.hpp>
#include <SushiDSP/foundation/core/rig_graph.hpp>
#include <SushiDSP/foundation/core/rt_swap_slot.hpp>
#include <SushiDSP/nodes/dd3/dd3_delay.hpp>
#include <SushiDSP/nodes/capture/metronome.hpp>
#include <SushiDSP/nodes/capture/track_recorder.hpp>
#include <SushiDSP/nodes/capture/track_player.hpp>
#include <SushiDSP/nodes/capture/track_timeline.hpp>
#include <SushiDSP/nodes/capture/vari_rate_reader.hpp>
#include <SushiDSP/foundation/audio_format/asio_sample_converter.hpp>
#include <SushiDSP/foundation/io/ir.hpp>
#include <SushiDSP/foundation/math/fft.hpp>
#include <SushiDSP/foundation/math/oversampler.hpp>
#include <SushiDSP/foundation/math/partitioned_convolution.hpp>
#include <SushiDSP/foundation/math/resampler.hpp>
#include <SushiDSP/foundation/math/streaming_convolution.hpp>
#include <SushiDSP/foundation/math/dk/damped_newton_solver.hpp>
#include <SushiDSP/foundation/math/dk/dk_circuit.hpp>
#include <SushiDSP/foundation/math/dk/dk_netlist.hpp>
#include <SushiDSP/nodes/routing/splitter_node.hpp>
#include <SushiDSP/nodes/routing/mixer_node.hpp>
#include <SushiDSP/nodes/routing/pan_node.hpp>
#include <SushiDSP/nodes/cabinet/cabinet_node.hpp>
#include <SushiDSP/nodes/diezel_vh4/diezel_vh4.hpp>
#include <SushiDSP/nodes/jcm800/jcm800.hpp>
#include <SushiDSP/nodes/mesa_mark2c/mesa_mark2c.hpp>
#include <SushiDSP/nodes/peavey5150/peavey5150.hpp>
#include <SushiDSP/nodes/room/binaural_decoder.hpp>
#include <SushiDSP/nodes/room/crossover_pair.hpp>
#include <SushiDSP/nodes/room/early_reflection_renderer.hpp>
#include <SushiDSP/nodes/room/early_tap_set.hpp>
#include <SushiDSP/nodes/room/fdn_design.hpp>
#include <SushiDSP/nodes/room/hrtf_set.hpp>
#include <SushiDSP/nodes/room/late_reverb.hpp>
#include <SushiDSP/nodes/room/modal_bank.hpp>
#include <SushiDSP/nodes/room/modal_design.hpp>
#include <SushiDSP/nodes/room/room_node.hpp>
#include <SushiDSP/nodes/room/spherical_head_hrtf.hpp>
#include <SushiDSP/nodes/tubescreamer/tube_screamer.hpp>

#include <SushiDSP/nodes/room_scenes/room_fixtures.hpp.hpp>
#include <SushiDSP/nodes/room_scenes/room_ports.hpp.hpp>
#include <SushiDSP/foundation/circuit_blocks/jcm_v1a_netlist.hpp.hpp>
#include "ui/level_probe.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{
    // Each thread carries its own trap, so a case only ever counts the thread that opened its window.
    thread_local std::atomic<bool> g_trap_allocations{false};
    thread_local std::atomic<int> g_trapped_allocation_count{0};

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
            ~AllocationTrap()
            {
                disarm();
            }

            AllocationTrap(const AllocationTrap&) = delete;
            AllocationTrap& operator=(const AllocationTrap&) = delete;

            /// @brief Opens the window, counting every allocation this thread makes from here.
            void arm() noexcept
            {
                g_trap_allocations.store(true, std::memory_order_relaxed);
            }

            /// @brief Closes the window.
            void disarm() noexcept
            {
                g_trap_allocations.store(false, std::memory_order_relaxed);
            }

            /// @brief Reports how many allocations this thread made inside the window.
            int count() const noexcept
            {
                return g_trapped_allocation_count.load(std::memory_order_relaxed);
            }
    };

    class SumNode : public SushiDSP::AudioNode
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

            SushiDSP::PortLayout ports() const override
            {
                return {0, 1};
            }

            void reset() override {}

            float get_parameter(const char*) const override
            {
                return 0.0f;
            }
            void set_parameter_for_control_thread(const char*, float) override {}
    };
} // namespace

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

    SushiDSP::PrepareInfo info{48000.0, 256, 1};
    graph.compile(info);

    std::vector<float> output_buffer(256, 0.0f);
    float* output_channels[1] = {output_buffer.data()};

    AllocationTrap trap;

    for (int block = 0; block < 7000; ++block)
    {
        for (std::uint16_t index = 0; index < 100; ++index)
        {
            graph.push_parameter_change(SushiDSP::ParameterChange{0, index, 0.5f});
        }

        trap.arm();

        SushiDSP::ProcessContext context{output_channels, nullptr, 256, 0.0};
        graph.process(context);

        trap.disarm();
    }

    EXPECT_EQ(trap.count(), 0);
}

namespace
{
    class StereoSourceNode : public SushiDSP::AudioNode
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

            SushiDSP::PortLayout ports() const override
            {
                return {0, 2};
            }

            void reset() override {}

            float get_parameter(const char*) const override
            {
                return 0.0f;
            }
            void set_parameter_for_control_thread(const char*, float) override {}
    };

    class MonoPassThroughNode : public SushiDSP::AudioNode
    {
        public:
            void prepare(const SushiDSP::PrepareInfo&) override {}

            void process(SushiDSP::ProcessContext& context) override
            {
                for (std::uint32_t frame = 0; frame < context.frames; ++frame)
                {
                    context.outputs[0][frame] =
                        context.inputs[0] != nullptr ? context.inputs[0][frame] : 0.0f;
                }
            }

            SushiDSP::PortLayout ports() const override
            {
                return {1, 1};
            }

            void reset() override {}

            float get_parameter(const char*) const override
            {
                return 0.0f;
            }
            void set_parameter_for_control_thread(const char*, float) override {}
    };
} // namespace

TEST(Unit_Dsp, RigGraphStereoProcessPerformsNoHeapAllocation)
{
    // The graph covers every path of the edge adaptation. See tests/functional/unit/README.md.
    SushiDSP::RigGraph graph(256, 4);
    const auto first_source = graph.add_node(std::make_unique<StereoSourceNode>());
    const auto second_source = graph.add_node(std::make_unique<StereoSourceNode>());
    const auto sink = graph.add_node(std::make_unique<MonoPassThroughNode>());
    graph.connect(first_source, sink);
    graph.connect(second_source, sink);

    SushiDSP::PrepareInfo info{48000.0, 256, 2};
    graph.compile(info);

    std::vector<float> left(256, 0.0f);
    std::vector<float> right(256, 0.0f);
    float* output_channels[2] = {left.data(), right.data()};

    AllocationTrap trap;

    for (int block = 0; block < 2000; ++block)
    {
        trap.arm();

        SushiDSP::ProcessContext context{output_channels, nullptr, 256, 0.0};
        graph.process(context);

        trap.disarm();
    }

    EXPECT_EQ(trap.count(), 0);
}

TEST(Unit_Dsp, FftForwardAndInverseRoundTripPerformsNoHeapAllocation)
{
    const std::uint32_t size = 256;
    SushiDSP::Fft fft(size);

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
        partition_size, impulse_response.data(),
        static_cast<std::uint32_t>(impulse_response.size()));

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

    SushiDSP::StreamingConvolution convolution(partition_size, impulse_response.data(),
                                               static_cast<std::uint32_t>(impulse_response.size()),
                                               4096);

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
    SushiDSP::PrepareInfo info{48000.0, 256, 1};
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
    const float* input_channels[1] = {input.data()};
    float* output_channels[1] = {output.data()};

    AllocationTrap trap;
    trap.arm();

    // A moved knob re-derives the matrices inside process(). See tests/functional/unit/README.md.
    for (int block = 0; block < 1000; ++block)
    {
        const float position = static_cast<float>(block) / 999.0f;
        node.set_drive(position);
        node.set_tone(1.0f - position);
        SushiDSP::ProcessContext context{output_channels, input_channels, 256, 0.0};
        node.process(context);
    }

    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
}

TEST(Unit_Dsp, SplitterNodeProcessPerformsNoHeapAllocation)
{
    SushiDSP::SplitterNode node;
    node.prepare(SushiDSP::PrepareInfo{48000.0, 256, 1});

    std::vector<float> input(256, 1.0f);
    std::vector<float> output(256, 0.0f);
    const float* inputs[1] = {input.data()};
    float* outputs[1] = {output.data()};
    SushiDSP::ProcessContext context{outputs, inputs, 256, 0.0};

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
    node.prepare(SushiDSP::PrepareInfo{48000.0, 256, 1});

    std::vector<float> a(256, 1.0f);
    std::vector<float> b(256, 1.0f);
    std::vector<float> output(256, 0.0f);
    const float* inputs[SushiDSP::MixerNode::K_MAX_FAN_IN] = {a.data(), b.data(), nullptr, nullptr,
                                                              nullptr,  nullptr,  nullptr, nullptr};
    float* outputs[1] = {output.data()};
    SushiDSP::ProcessContext context{outputs, inputs, 256, 0.0};

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
    node.prepare(SushiDSP::PrepareInfo{48000.0, 256, 1});
    node.set_parameter_for_control_thread("pan", 0.5f);

    std::vector<float> input(256, 1.0f);
    std::vector<float> out_left(256, 0.0f);
    std::vector<float> out_right(256, 0.0f);
    const float* inputs[1] = {input.data()};
    float* outputs[2] = {out_left.data(), out_right.data()};
    SushiDSP::ProcessContext context{outputs, inputs, 256, 0.0};

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
    // The per-block set_high_shelf() is pure arithmetic. See tests/functional/unit/README.md.
    SushiDSP::Jcm800 node;
    SushiDSP::PrepareInfo info{48000.0, 256, 1};
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
    const float* input_channels[1] = {input.data()};
    float* output_channels[1] = {output.data()};

    AllocationTrap trap;
    trap.arm();

    for (int block = 0; block < 500; ++block)
    {
        SushiDSP::ProcessContext context{output_channels, input_channels, 256, 0.0};
        node.process(context);
    }

    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
}

TEST(Unit_Dsp, Peavey5150ProcessPerformsNoHeapAllocationWithoutCabinet)
{
    SushiDSP::Peavey5150 node;
    SushiDSP::PrepareInfo info{48000.0, 256, 1};
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
    const float* input_channels[1] = {input.data()};
    float* output_channels[1] = {output.data()};

    AllocationTrap trap;
    trap.arm();

    for (int block = 0; block < 500; ++block)
    {
        SushiDSP::ProcessContext context{output_channels, input_channels, 256, 0.0};
        node.process(context);
    }

    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
}

TEST(Unit_Dsp, MesaMark2cProcessPerformsNoHeapAllocationWithoutCabinet)
{
    SushiDSP::MesaMark2c node;
    SushiDSP::PrepareInfo info{48000.0, 256, 1};
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
    const float* input_channels[1] = {input.data()};
    float* output_channels[1] = {output.data()};

    AllocationTrap trap;
    trap.arm();

    for (int block = 0; block < 500; ++block)
    {
        SushiDSP::ProcessContext context{output_channels, input_channels, 256, 0.0};
        node.process(context);
    }

    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
}

TEST(Unit_Dsp, DiezelVh4ProcessPerformsNoHeapAllocationWithoutCabinet)
{
    SushiDSP::DiezelVh4 node;
    SushiDSP::PrepareInfo info{48000.0, 256, 1};
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
    const float* input_channels[1] = {input.data()};
    float* output_channels[1] = {output.data()};

    AllocationTrap trap;
    trap.arm();

    for (int block = 0; block < 500; ++block)
    {
        SushiDSP::ProcessContext context{output_channels, input_channels, 256, 0.0};
        node.process(context);
    }

    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
}

TEST(Unit_Dsp, CabinetNodeProcessPerformsNoHeapAllocationWithCabinet)
{
    // 500 blocks wrap the spectrum history at least once. See tests/functional/unit/README.md.
    std::vector<float> impulse_response(200, 0.0f);
    impulse_response[0] = 1.0f;
    for (std::size_t i = 1; i < impulse_response.size(); ++i)
    {
        impulse_response[i] = 0.5f * impulse_response[i - 1];
    }

    SushiDSP::CabinetNode node;
    SushiDSP::PrepareInfo info{48000.0, 256, 1};
    node.prepare(info);
    ASSERT_TRUE(node.set_cabinet(SushiDSP::Ir{impulse_response, 48000.0}));

    std::vector<float> input(256);
    for (std::size_t i = 0; i < input.size(); ++i)
    {
        input[i] = 0.2f * std::sin(2.0f * static_cast<float>(M_PI) * 220.0f * i / 48000.0f);
    }
    std::vector<float> output(256, 0.0f);
    const float* input_channels[1] = {input.data()};
    float* output_channels[1] = {output.data()};

    AllocationTrap trap;
    trap.arm();

    for (int block = 0; block < 500; ++block)
    {
        SushiDSP::ProcessContext context{output_channels, input_channels, 256, 0.0};
        node.process(context);
    }

    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
}

TEST(Unit_Dsp, AsioSampleConverterPerformsNoHeapAllocation)
{
    std::uint8_t bytes[4] = {0, 0, 0, 0};

    AllocationTrap trap;
    trap.arm();

    for (int i = 0; i < 10000; ++i)
    {
        const float decoded =
            SushiDSP::asio_sample_to_float(SushiDSP::AsioSampleFormat::Int32LSB, bytes);
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

TEST(Unit_Dsp, CabinetNodeSetCabinetSwapsWithoutAllocatingOnTheAudioThread)
{
    // The trap is armed around the blocks, not set_cabinet(). See tests/functional/unit/README.md.
    SushiDSP::CabinetNode node;
    SushiDSP::PrepareInfo info{48000.0, 256, 1};
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
    const float* input_channels[1] = {input.data()};
    float* output_channels[1] = {output.data()};

    AllocationTrap trap;

    for (int block = 0; block < 4; ++block)
    {
        impulse_response.samples[0] = 1.0f - 0.1f * static_cast<float>(block);
        ASSERT_TRUE(node.set_cabinet(impulse_response));
        ASSERT_TRUE(node.has_cabinet());

        trap.arm();
        SushiDSP::ProcessContext context{output_channels, input_channels, 256, 0.0};
        node.process(context);
        trap.disarm();
    }

    EXPECT_EQ(trap.count(), 0);

    node.reclaim_cabinets();
}

namespace
{
    // The netlist lives in modules/foundation/circuit_blocks/include/SushiDSP/foundation/circuit_blocks/jcm_v1a_netlist.hpp. See tests/functional/unit/README.md.
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
            void residual_and_jacobian(const std::array<double, 2>& v,
                                       std::array<double, 2>& residual,
                                       std::array<std::array<double, 2>, 2>& jacobian) const
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
} // namespace

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
    TriodeStagePort port;
    ASSERT_TRUE(circuit.prepare(netlist, 48000.0));

    const std::array<double, 2> silence{0.0, K_SUPPLY_VOLTS};
    std::array<double, 1> outputs{};
    for (int sample = 0; sample < 4800; ++sample)
    {
        circuit.process_sample(port, silence, outputs);
    }

    AllocationTrap trap;
    trap.arm();

    for (int sample = 0; sample < 256; ++sample)
    {
        const double drive =
            0.2 * std::sin(2.0 * M_PI * 220.0 * static_cast<double>(sample) / 48000.0);
        const std::array<double, 2> inputs{drive, K_SUPPLY_VOLTS};
        circuit.process_sample(port, inputs, outputs);
    }

    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
    EXPECT_EQ(circuit.nan_fallback_samples(), 0u);
}

TEST(Unit_Dsp, DkCircuitPotentiometerUpdatePerformsNoHeapAllocation)
{
    // A pot move rebuilds the DK matrices on the audio thread. See tests/functional/unit/README.md.
    DkStageNetlist netlist;
    const std::size_t pot_index = build_jcm_v1a(netlist);

    DkStageCircuit circuit;
    TriodeStagePort port;
    ASSERT_TRUE(circuit.prepare(netlist, 48000.0));

    const std::array<double, 2> silence{0.0, K_SUPPLY_VOLTS};
    std::array<double, 1> outputs{};

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
    ExponentialPairSystem system;
    std::array<double, 2> v{0.0, 0.0};

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
    // At the slowest clock interpolate() touches 164 taps. See tests/functional/unit/README.md.
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
    // One 256-frame block calls set_rates 32 times. See tests/functional/unit/README.md.
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
            const double clock_hz =
                20000.0 + (82500.0 - 20000.0) * static_cast<double>(sub_block) / 31.0;
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
    // Both MODE changes below move N and pay the std::fill. See tests/functional/unit/README.md.
    SushiDSP::Dd3Delay node;
    node.prepare(SushiDSP::PrepareInfo{48000.0, 256, 1});
    node.set_time(0.4f);
    node.set_feedback(0.45f);
    node.set_level(0.5f);

    std::vector<float> input(256);
    for (std::size_t i = 0; i < input.size(); ++i)
    {
        input[i] = 0.3f * std::sin(2.0f * static_cast<float>(M_PI) * 220.0f * i / 48000.0f);
    }
    std::vector<float> output(256, 0.0f);
    const float* input_channels[1] = {input.data()};
    float* output_channels[1] = {output.data()};
    SushiDSP::ProcessContext context{output_channels, input_channels, 256, 0.0};

    const std::uint16_t mode_index = 3u;
    const float long_mode_value =
        static_cast<float>(static_cast<int>(SushiDSP::dd3::Dd3Mode::Long));

    AllocationTrap trap;
    trap.arm();

    node.process(context);
    node.set_parameter_for_audio_thread(mode_index, long_mode_value);
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
    // The trap sees the ring copy and the drop counter. See tests/functional/unit/README.md.
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "sushidsp_rt_safety_recorder";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    SushiDSP::TrackRecorder recorder(2u, 48000.0, 65536);
    ASSERT_TRUE(recorder.arm((root / "rt_safety.wav").string()));

    std::vector<float> left(256, 0.25f);
    std::vector<float> right(256, -0.25f);
    const float* channels[2] = {left.data(), right.data()};

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
    // Timelines and converters exist before the trap opens. See tests/functional/unit/README.md.
    SushiDSP::TrackPlayer player;
    SushiDSP::Transport transport;

    for (std::size_t track = 0; track < SushiDSP::TrackPlayer::K_MAX_TRACKS; ++track)
    {
        auto take = std::make_unique<SushiDSP::Take>(track % 2 == 0 ? 1u : 2u, 48000.0, 4096u);
        float* samples = take->mutable_channel(0);
        for (std::size_t frame = 0; frame < 4096; ++frame)
        {
            samples[frame] = 0.25f * std::sin(2.0f * static_cast<float>(M_PI) * 110.0f *
                                              static_cast<float>(frame) / 48000.0f);
        }

        const SushiDSP::Take* borrowed =
            player.takes().insert(std::unique_ptr<const SushiDSP::Take>(std::move(take)));

        std::vector<SushiDSP::Clip> clips;
        for (std::size_t clip = 0; clip < SushiDSP::TrackTimeline::K_MAX_CLIPS; ++clip)
        {
            clips.push_back(SushiDSP::Clip{borrowed, clip * 200u, 64u, 150u});
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
    float* outputs[2] = {left.data(), right.data()};

    AllocationTrap trap;
    trap.arm();

    for (double speed : {1.0, 0.5})
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
    // The trap covers the pick-up of a changed lane. See tests/functional/unit/README.md.
    SushiDSP::TrackPlayer player;
    SushiDSP::Transport transport;

    auto take = std::make_unique<SushiDSP::Take>(2u, 48000.0, 4096u);
    float* samples = take->mutable_channel(0);
    for (std::size_t frame = 0; frame < 4096; ++frame)
    {
        samples[frame] = 0.25f * std::sin(2.0f * static_cast<float>(M_PI) * 110.0f *
                                          static_cast<float>(frame) / 48000.0f);
    }
    const SushiDSP::Take* borrowed =
        player.takes().insert(std::unique_ptr<const SushiDSP::Take>(std::move(take)));

    constexpr int K_SWAPS = 64;
    std::vector<std::unique_ptr<SushiDSP::TrackTimeline>> prepared;
    for (int swap = 0; swap < K_SWAPS; ++swap)
    {
        std::vector<SushiDSP::Clip> clips;
        for (std::size_t clip = 0; clip < SushiDSP::TrackTimeline::K_MAX_CLIPS; ++clip)
        {
            clips.push_back(SushiDSP::Clip{borrowed, static_cast<std::uint64_t>(swap) + clip * 300u,
                                           32u, 250u});
        }
        prepared.push_back(std::unique_ptr<SushiDSP::TrackTimeline>(
            new SushiDSP::TrackTimeline(player.takes(), clips)));
    }

    player.prepare(256u);
    player.set_pan(0, -0.5f);
    transport.set_speed(0.75);

    std::vector<float> left(256, 0.0f);
    std::vector<float> right(256, 0.0f);
    float* outputs[2] = {left.data(), right.data()};

    AllocationTrap trap;

    for (int swap = 0; swap < K_SWAPS; ++swap)
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
    // The converters are sized before the trap opens. See tests/functional/unit/README.md.
    SushiDSP::Take take(2u, 48000.0, 48000u);
    for (std::uint32_t channel = 0; channel < 2u; ++channel)
    {
        float* samples = take.mutable_channel(channel);
        for (std::size_t frame = 0; frame < 48000u; ++frame)
        {
            samples[frame] = 0.25f * std::sin(2.0f * static_cast<float>(M_PI) * 110.0f *
                                              static_cast<float>(frame) / 48000.0f);
        }
    }

    SushiDSP::VariRateReader reader;
    reader.prepare(2u, 256u);

    std::vector<float> left(256, 0.0f);
    std::vector<float> right(256, 0.0f);
    float* outputs[2] = {left.data(), right.data()};

    AllocationTrap trap;
    trap.arm();

    for (double speed : {0.5, 1.0, 2.0})
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
    // The bursts are built before the trap opens. See tests/functional/unit/README.md.
    SushiDSP::MusicalClock clock;
    clock.set_tempo(137.4);

    SushiDSP::Metronome metronome(48000.0);
    metronome.set_enabled(true);
    metronome.set_level(0.5f);

    SushiDSP::Transport transport;
    transport.play();

    std::vector<float> left(256, 0.0f);
    std::vector<float> right(256, 0.0f);
    float* outputs[2] = {left.data(), right.data()};

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
        scene.source_position = {1.5, 2.0, 0.5};
        scene.microphone_position = {microphone_x, 2.0, 1.2};
        scene.microphone_pattern = SushiDSP::room::PolarPattern::Omni;
        return scene;
    }

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

TEST(Unit_Dsp, RoomNodeProcessPerformsNoHeapAllocation)
{
    // The first trapped block adopts the published set. See tests/functional/unit/README.md.
    SushiDSP::room::RoomNode node;
    node.prepare(SushiDSP::PrepareInfo{48000.0, 256, 1});
    std::string reason;
    ASSERT_TRUE(node.set_scene(room_trap_scene(4.0), reason)) << reason;

    std::vector<float> input(256);
    fill_room_tone(input);
    std::vector<float> left(256, 0.0f);
    std::vector<float> right(256, 0.0f);
    const float* input_channels[1] = {input.data()};
    float* output_channels[2] = {left.data(), right.data()};
    SushiDSP::ProcessContext context{output_channels, input_channels, 256, 0.0};

    AllocationTrap trap;
    trap.arm();
    for (int block = 0; block < 16; ++block)
    {
        node.set_parameter_for_audio_thread(std::uint16_t{0}, block % 2 == 0 ? 0.0f : 1.0f);
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
    // Only the audio thread's half of a publish is trapped. See tests/functional/unit/README.md.
    SushiDSP::room::RoomNode node;
    node.prepare(SushiDSP::PrepareInfo{48000.0, 256, 1});

    std::vector<float> input(256);
    fill_room_tone(input);
    const float* input_channels[2] = {input.data(), input.data()};
    SushiDSP::test::RoomPorts ports(256);
    SushiDSP::ProcessContext context = ports.context(input_channels, 256);

    AllocationTrap trap;
    for (int publish = 0; publish < 4; ++publish)
    {
        SushiDSP::room::RoomScene scene = room_trap_scene(3.0 + 0.25 * publish);
        scene.listener_position.y = 2.0 + 0.1 * (publish % 2);
        scene.dimensions.x = publish < 2 ? 6.0 : 6.2;
        SushiDSP::Room::Arrangement arrangement = SushiDSP::Room::Arrangement::from_scene(scene);
        SushiDSP::Room::MicrophonePlacement microphone;
        microphone.id = 1;
        microphone.pose = {scene.microphone_position, scene.microphone_yaw_rad};
        microphone.pattern = scene.microphone_pattern;
        std::string reason;
        ASSERT_TRUE(arrangement.add_microphone(microphone, reason)) << reason;
        ASSERT_TRUE(node.set_arrangement(arrangement, reason)) << reason;

        trap.arm();
        for (int block = 0; block < 3; ++block)
        {
            node.process(context);
        }
        trap.disarm();

        node.reclaim_rooms();
    }

    EXPECT_EQ(trap.count(), 0);
    for (std::size_t i = 0; i < 256; ++i)
    {
        EXPECT_TRUE(std::isfinite(ports.listener(0)[i]));
        EXPECT_TRUE(std::isfinite(ports.microphone(0)[i]));
    }
}

TEST(Unit_Dsp, RoomNodeProcessPerformsNoHeapAllocationAtTheLimits)
{
    // Three cabinets and eight microphones, with placements coming and going between blocks.
    SushiDSP::room::RoomNode node;
    node.prepare(SushiDSP::PrepareInfo{48000.0, 256, 1});

    std::vector<float> input(256);
    fill_room_tone(input);
    const float* input_channels[6] = {input.data(), input.data(), input.data(),
                                      input.data(), input.data(), input.data()};
    SushiDSP::test::RoomPorts ports(256);
    SushiDSP::ProcessContext context = ports.context(input_channels, 256);

    AllocationTrap trap;
    const std::uint32_t source_counts[4] = {3, 3, 1, 3};
    const std::uint32_t microphone_counts[4] = {8, 8, 2, 8};
    for (int publish = 0; publish < 4; ++publish)
    {
        SushiDSP::Room::Arrangement arrangement = SushiDSP::test::spread_arrangement(
            SushiDSP::test::garage_scene(), source_counts[publish], microphone_counts[publish]);
        if (publish == 1)
        {
            SushiDSP::Room::MicrophonePlacement moved = arrangement.microphones().front();
            moved.pose.position.x += 0.2;
            ASSERT_TRUE(arrangement.update_microphone(moved));
        }
        std::string reason;
        ASSERT_TRUE(node.set_arrangement(arrangement, reason)) << reason;

        trap.arm();
        for (int block = 0; block < 4; ++block)
        {
            node.process(context);
        }
        trap.disarm();

        node.reclaim_rooms();
    }

    EXPECT_EQ(trap.count(), 0);
    for (std::size_t i = 0; i < 256; ++i)
    {
        EXPECT_TRUE(std::isfinite(ports.listener(0)[i]));
        EXPECT_TRUE(std::isfinite(ports.listener(1)[i]));
        EXPECT_TRUE(std::isfinite(ports.microphone(7)[i]));
    }
}

TEST(Unit_Dsp, EarlyReflectionRendererRenderPerformsNoHeapAllocation)
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

    AllocationTrap trap;
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

TEST(Unit_Dsp, ModalBankProcessPerformsNoHeapAllocation)
{
    // All K_MAX_MODES resonators run at 68.6 Hz, 1 s decay. See tests/functional/unit/README.md.
    SushiDSP::room::ModalBank bank;
    bank.prepare(256);

    auto design = std::make_unique<SushiDSP::room::ModalDesign>();
    const double radius = std::pow(10.0, -3.0 / 48000.0);
    const double theta = 2.0 * M_PI * 68.6 / 48000.0;
    design->count = static_cast<std::uint32_t>(SushiDSP::room::ModalDesign::K_MAX_MODES);
    for (std::uint32_t mode = 0; mode < design->count; ++mode)
    {
        design->modes[mode].section = {1e-3, 0.0, -1e-3, -2.0 * radius * std::cos(theta),
                                       radius * radius};
        design->modes[mode].source_gain = 0.5f;
        design->modes[mode].ear_gain = {0.7f, -0.7f};
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
    // set_crossover is arithmetic on the audio thread. See tests/functional/unit/README.md.
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
    // The trapped blocks include the 512-frame crossfade. See tests/functional/unit/README.md.
    SushiDSP::room::LateReverb reverb;
    reverb.prepare(25200);

    SushiDSP::room::FdnDesign design{};
    const SushiDSP::room::FdnDesign::Section unity = {1.0, 0.0, 0.0, 0.0, 0.0};
    const SushiDSP::room::FdnDesign::Section damped = {0.9, 0.0, 0.0, 0.0, 0.0};
    for (std::size_t line = 0; line < SushiDSP::room::FdnDesign::K_LINE_COUNT; ++line)
    {
        design.line_frames[line] = static_cast<std::uint32_t>(1000 + 97 * line);
        design.line_section_a[line] = damped;
        design.line_section_b[line] = unity;
    }
    design.tone_correction = {unity, unity};
    design.coherence_a = {unity, unity};
    design.coherence_b = {unity, unity};
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

TEST(Unit_Dsp, BinauralDecoderProcessPerformsNoHeapAllocation)
{
    // Sixteen blocks cross the 128-frame partition 32 times. See tests/functional/unit/README.md.
    const SushiDSP::room::HrtfSet set = SushiDSP::room::SphericalHeadHrtf::build(48000.0);
    SushiDSP::room::BinauralDecoder decoder;
    decoder.prepare(set, 48000.0, 256);
    ASSERT_TRUE(decoder.ready());

    std::array<std::vector<float>, SushiDSP::room::BinauralDecoder::K_CHANNELS> channels;
    std::array<const float*, SushiDSP::room::BinauralDecoder::K_CHANNELS> inputs{};
    for (std::size_t channel = 0; channel < channels.size(); ++channel)
    {
        channels[channel].assign(256, 0.0f);
        fill_room_tone(channels[channel]);
        inputs[channel] = channels[channel].data();
    }
    std::vector<float> left(256, 0.0f);
    std::vector<float> right(256, 0.0f);

    AllocationTrap trap;
    trap.arm();
    for (int block = 0; block < 16; ++block)
    {
        decoder.process(inputs.data(), left.data(), right.data(), 256);
    }
    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
    for (std::size_t i = 0; i < left.size(); ++i)
    {
        EXPECT_TRUE(std::isfinite(left[i]));
        EXPECT_TRUE(std::isfinite(right[i]));
    }
}

TEST(Unit_Dsp, RoomNodeBinauralProcessPerformsNoHeapAllocation)
{
    // The ears render every binaural path inside the trap. See tests/functional/unit/README.md.
    SushiDSP::room::RoomNode node;
    node.prepare(SushiDSP::PrepareInfo{48000.0, 256, 1});
    std::string reason;
    ASSERT_TRUE(node.set_scene(room_trap_scene(4.0), reason)) << reason;

    std::vector<float> input(256);
    fill_room_tone(input);
    std::vector<float> left(256, 0.0f);
    std::vector<float> right(256, 0.0f);
    const float* input_channels[1] = {input.data()};
    float* output_channels[2] = {left.data(), right.data()};
    SushiDSP::ProcessContext context{output_channels, input_channels, 256, 0.0};

    AllocationTrap trap;
    for (int publish = 0; publish < 3; ++publish)
    {
        if (publish == 1)
        {
            node.set_parameter_for_control_thread("direct_delay", 1.0f);
        }
        if (publish == 2)
        {
            ASSERT_TRUE(node.set_hrtf(SushiDSP::room::HrtfSet{}));
        }
        trap.arm();
        for (int block = 0; block < 4; ++block)
        {
            node.set_parameter_for_audio_thread(std::uint16_t{3}, block % 2 == 0 ? 1.0f : 0.0f);
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
