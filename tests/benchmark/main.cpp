/**************************************************************************/
/* main.cpp                                                               */
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
/* permissions and limitations under the License.                        */
/**************************************************************************/

// _USE_MATH_DEFINES must precede every include: it is what makes MSVC's <cmath> declare M_PI.
#define _USE_MATH_DEFINES
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#include "Benchmark.hpp"
#include "SushiDSP/core/DenormalGuard.hpp"
#include "SushiDSP/core/IAudioNode.hpp"
#include "SushiDSP/jcm800/Jcm800.hpp"
#include "SushiDSP/jcm800/TriodeStage.hpp"
#include "SushiDSP/math/Oversampler.hpp"
#include "SushiDSP/math/PartitionedConvolution.hpp"
#include "SushiDSP/tubescreamer/TubeScreamer.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{
    constexpr double kSampleRate = 48000.0;
    constexpr std::uint32_t kBlockSize = 256;

    /// @brief The 220 Hz tone the regression snapshots use, so the bench exercises the same path.
    std::vector<float> make_test_tone(std::uint32_t frames)
    {
        std::vector<float> tone(frames);
        for (std::uint32_t i = 0; i < frames; ++i)
        {
            tone[i] = 0.25f * static_cast<float>(
                std::sin(2.0 * M_PI * 220.0 * static_cast<double>(i) / kSampleRate));
        }
        return tone;
    }

    /// @brief Times one node's process() over a block, the way a device callback calls it.
    template <typename Node, typename Configure>
    SushiDSP::bench::Result measure_node(const char* name, Configure&& configure)
    {
        Node node;
        node.prepare({ kSampleRate, kBlockSize, 1 });
        configure(node);
        const std::vector<float> input = make_test_tone(kBlockSize);
        std::vector<float> output(kBlockSize, 0.0f);
        const float* inputs[1] = { input.data() };
        float* outputs[1] = { output.data() };
        return SushiDSP::bench::measure(name, kBlockSize, kSampleRate, [&]
        {
            SushiDSP::ProcessContext context{ outputs, inputs, kBlockSize, 0.0 };
            node.process(context);
        });
    }

    /// @brief With no arguments every case runs; otherwise a case runs when any argument is a
    /// substring of its name.
    bool selected(const char* name, int argc, char** argv)
    {
        if (argc < 2)
        {
            return true;
        }
        for (int i = 1; i < argc; ++i)
        {
            if (std::strstr(name, argv[i]) != nullptr)
            {
                return true;
            }
        }
        return false;
    }
}

int main(int argc, char** argv)
{
    using SushiDSP::bench::print;
    // Same FPU mode the device callbacks run under, so the numbers match the real thread.
    SushiDSP::DenormalGuard denormal_guard;
    std::printf("SushiDSP benchmark  (%g Hz, %u-frame blocks, best of 200)\n\n", kSampleRate, kBlockSize);

    if (selected("Jcm800", argc, argv))
    {
        print(measure_node<SushiDSP::Jcm800>("Jcm800 (no cabinet)", [](SushiDSP::Jcm800& node)
        {
            node.set_preamp(0.6f);
            node.set_master(0.0f);
            node.set_bass(0.6f);
            node.set_middle(0.4f);
            node.set_treble(0.7f);
            node.set_presence(0.5f);
        }));
    }
    if (selected("TubeScreamer", argc, argv))
    {
        print(measure_node<SushiDSP::TubeScreamer>("TubeScreamer", [](SushiDSP::TubeScreamer& node)
        {
            node.set_drive(0.7f);
            node.set_tone(0.5f);
            node.set_level(0.8f);
        }));
    }
    if (selected("Oversampler8x", argc, argv))
    {
        SushiDSP::Oversampler8x oversampler(kBlockSize);
        const std::vector<float> input = make_test_tone(kBlockSize);
        std::vector<float> output(kBlockSize, 0.0f);
        print(SushiDSP::bench::measure("Oversampler8x round trip", kBlockSize, kSampleRate, [&]
        {
            const float* upsampled = oversampler.upsample(input.data(), kBlockSize);
            oversampler.downsample(upsampled, kBlockSize * 8, output.data());
        }));
    }
    if (selected("TriodeStage", argc, argv))
    {
        // The 12AX7 values Jcm800 uses for V1a.
        SushiDSP::TriodeStage triode(10000.0f, 1000000.0f, -1.5f, 60.0f, 1e-9f, 0.02585f);
        const std::vector<float> input = make_test_tone(kBlockSize);
        std::vector<float> output(kBlockSize, 0.0f);
        print(SushiDSP::bench::measure("TriodeStage (base rate)", kBlockSize, kSampleRate, [&]
        {
            for (std::uint32_t i = 0; i < kBlockSize; ++i)
            {
                output[i] = triode.process_sample(input[i]);
            }
        }));
    }
    if (selected("PartitionedConvolution", argc, argv))
    {
        constexpr std::uint32_t impulse_length = 4096;
        std::vector<float> impulse(impulse_length);
        for (std::uint32_t i = 0; i < impulse_length; ++i)
        {
            impulse[i] = std::exp(-static_cast<float>(i) / 1000.0f)
                * static_cast<float>(std::sin(0.1 * static_cast<double>(i)));
        }
        SushiDSP::PartitionedConvolution convolution(kBlockSize, impulse.data(), impulse_length);
        const std::vector<float> input = make_test_tone(kBlockSize);
        std::vector<float> output(kBlockSize, 0.0f);
        print(SushiDSP::bench::measure("PartitionedConvolution (4096 IR)", kBlockSize, kSampleRate, [&]
        {
            convolution.process(input.data(), output.data());
        }));
    }
    return 0;
}
