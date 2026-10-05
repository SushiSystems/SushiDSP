/****************************************************************/
/* SineToneNode.cpp                                             */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

#include "SineToneNode.hpp"

#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace SushiDSP
{
    SineToneNode::SineToneNode(float frequency_hz)
        : frequency_hz_(frequency_hz)
    {
    }

    void SineToneNode::prepare(const PrepareInfo& info)
    {
        sample_rate_ = info.sample_rate;
    }

    void SineToneNode::process(ProcessContext& context)
    {
        const double phase_increment = 2.0 * M_PI * static_cast<double>(frequency_hz_) / sample_rate_;

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
        return { 0, 1 };
    }

    void SineToneNode::reset()
    {
        phase_ = 0.0;
    }
}
