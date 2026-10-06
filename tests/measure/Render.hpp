/**************************************************************************/
/* Render.hpp                                                             */
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
 * @file Render.hpp
 * @brief Defines the block-by-block node renderer and the sine stimulus it is fed.
 * @author Mustafa Garip
 */

#pragma once

#define _USE_MATH_DEFINES
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "SushiDSP/core/IAudioNode.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace SushiDSP::measure
{
    /// @brief Prepares a node and streams a stimulus through it block by block.
    /// @param node Any mono node; its knobs should already be set so prepare() snaps them.
    /// @param stimulus Input samples.
    /// @param sample_rate Rate passed to prepare().
    /// @param block_size Maximum block passed to prepare() and used for every full block.
    /// @return The node's output, stimulus.size() samples.
    inline std::vector<float> render(IAudioNode& node, const std::vector<float>& stimulus, double sample_rate, std::uint32_t block_size)
    {
        node.prepare({ sample_rate, block_size, 1 });
        std::vector<float> output(stimulus.size(), 0.0f);
        std::size_t position = 0;
        while (position < stimulus.size())
        {
            const std::uint32_t frames = static_cast<std::uint32_t>(std::min<std::size_t>(block_size, stimulus.size() - position));
            const float* inputs[1] = { stimulus.data() + position };
            float* outputs[1] = { output.data() + position };
            ProcessContext context{ outputs, inputs, frames, static_cast<double>(position) / sample_rate };
            node.process(context);
            position += frames;
        }
        return output;
    }

    /// @brief Sine of `length` samples.
    /// @param frequency_hz Frequency.
    /// @param sample_rate Sample rate.
    /// @param length Number of samples.
    /// @param amplitude Peak amplitude.
    /// @return The samples.
    inline std::vector<float> sine_stimulus(double frequency_hz, double sample_rate, std::size_t length, float amplitude)
    {
        std::vector<float> signal(length);
        for (std::size_t i = 0; i < length; ++i)
        {
            signal[i] = amplitude * static_cast<float>(std::sin(2.0 * M_PI * frequency_hz * static_cast<double>(i) / sample_rate));
        }
        return signal;
    }
}
