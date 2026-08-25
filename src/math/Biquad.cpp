// Biquad.cpp
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

// _USE_MATH_DEFINES must precede every include: it is what makes MSVC's <cmath> declare M_PI,
// and it must remain the very first thing in the translation unit.
#define _USE_MATH_DEFINES

#include "SushiDSP/math/Biquad.hpp"

#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace SushiDSP
{
    void Biquad::set_low_pass(double sample_rate, double cutoff_hz, double q)
    {
        const double omega = 2.0 * M_PI * cutoff_hz / sample_rate;
        const double sin_omega = std::sin(omega);
        const double cos_omega = std::cos(omega);
        const double alpha = sin_omega / (2.0 * q);

        const double b0 = (1.0 - cos_omega) / 2.0;
        const double b1 = 1.0 - cos_omega;
        const double b2 = (1.0 - cos_omega) / 2.0;
        const double a0 = 1.0 + alpha;
        const double a1 = -2.0 * cos_omega;
        const double a2 = 1.0 - alpha;

        b0_ = static_cast<float>(b0 / a0);
        b1_ = static_cast<float>(b1 / a0);
        b2_ = static_cast<float>(b2 / a0);
        a1_ = static_cast<float>(a1 / a0);
        a2_ = static_cast<float>(a2 / a0);
    }

    float Biquad::process_sample(float input)
    {
        const float output = b0_ * input + b1_ * x1_ + b2_ * x2_ - a1_ * y1_ - a2_ * y2_;

        x2_ = x1_;
        x1_ = input;
        y2_ = y1_;
        y1_ = output;

        return output;
    }

    void Biquad::reset()
    {
        x1_ = x2_ = y1_ = y2_ = 0.0f;
    }
}
