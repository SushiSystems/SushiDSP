/**************************************************************************/
/* sine_tone_node.cpp                                                     */
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
 * @file sine_tone_node.cpp
 * @brief Implements the sine generator the standalone host plays its test tone from.
 * @author Mustafa Garip
 */

#include "sine_tone_node.hpp"

#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace SushiDSP
{
    SineToneNode::SineToneNode(float frequency_hz) : frequency_hz_(frequency_hz) {}

    void SineToneNode::prepare(const PrepareInfo& info)
    {
        sample_rate_ = info.sample_rate;
    }

    void SineToneNode::process(ProcessContext& context)
    {
        const double phase_increment =
            2.0 * M_PI * static_cast<double>(frequency_hz_) / sample_rate_;

        for (std::uint32_t frame = 0; frame < context.frames; ++frame)
        {
            context.outputs[0][frame] = static_cast<float>(std::sin(phase_));
            phase_ += phase_increment;
            if (phase_ >= 2.0 * M_PI)
            {
                phase_ -= 2.0 * M_PI;
            }
        }
    }

    PortLayout SineToneNode::ports() const
    {
        return {0, 1};
    }

    void SineToneNode::reset()
    {
        phase_ = 0.0;
    }
} // namespace SushiDSP
