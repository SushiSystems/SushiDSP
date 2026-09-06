/**************************************************************************/
/* FmvToneStack.cpp                                                       */
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

/**
 * @file FmvToneStack.cpp
 * @brief Continuous-time coefficients of the FMV stack and their bilinear discretisation.
 * @author Mustafa Garip & Sushi Systems
 */

#include "SushiDSP/circuits/FmvToneStack.hpp"

#include <algorithm>

namespace SushiDSP
{
    FmvToneStack::FmvToneStack(double sample_rate, const FmvComponents& components)
        : sample_rate_(sample_rate), components_(components)
    {
        recompute();
    }

    void FmvToneStack::set_bass(float normalized)
    {
        bass_ = std::clamp(normalized, 0.0f, 1.0f);
        recompute();
    }

    void FmvToneStack::set_middle(float normalized)
    {
        middle_ = std::clamp(normalized, 0.0f, 1.0f);
        recompute();
    }

    void FmvToneStack::set_treble(float normalized)
    {
        treble_ = std::clamp(normalized, 0.0f, 1.0f);
        recompute();
    }

    void FmvToneStack::recompute()
    {
        // r1 is the treble pot and r4 the slope resistor, as the design document numbers them.
        const double c1 = components_.treble_capacitor_farads;
        const double c2 = components_.bass_capacitor_farads;
        const double c3 = components_.middle_capacitor_farads;
        const double r1 = components_.treble_potentiometer_ohms;
        const double r2 = components_.bass_potentiometer_ohms;
        const double r3 = components_.middle_potentiometer_ohms;
        const double r4 = components_.slope_resistor_ohms;

        const double l = bass_;
        const double m = middle_;
        const double t = treble_;

        const double b1 = t * c1 * r1 + l * (c1 * r2 + c2 * r2) + m * (c1 * r3 + c2 * r3 + c3 * r3);
        const double b2 = t * (c1 * c2 * r1 * r4 + c1 * c3 * r1 * r4)
                        + l * (c1 * c2 * r1 * r2 + c1 * c2 * r2 * r4 + c1 * c3 * r2 * r4
                               + m * (c1 * c3 * r2 * r3 + c2 * c3 * r2 * r3))
                        + m * (c1 * c2 * r1 * r3 + c1 * c2 * r3 * r4 + c1 * c3 * r1 * r3 + c1 * c3 * r3 * r4);
        const double b3 = l * c1 * c2 * c3 * r2 * (t * r1 * r4 + m * (r1 * r3 + r3 * r4));

        const double a1 = c1 * r1 + c2 * r4 + c3 * r4 + l * (c1 * r2 + c2 * r2)
                        + m * (c1 * r3 + c2 * r3 + c3 * r3);
        const double a2 = c1 * c2 * r1 * r4 + c1 * c3 * r1 * r4
                        + l * (c1 * c2 * r1 * r2 + c1 * c2 * r2 * r4 + c1 * c3 * r2 * r4 + c2 * c3 * r2 * r4
                               + m * (c1 * c3 * r2 * r3 + c2 * c3 * r2 * r3))
                        + m * (c1 * c2 * r1 * r3 + c1 * c2 * r3 * r4 + c1 * c3 * r1 * r3 + c1 * c3 * r3 * r4);
        const double a3 = l * c1 * c2 * c3 * r2 * (r1 * r4 + m * (r1 * r3 + r3 * r4));

        const double c = 2.0 * sample_rate_;
        const double c_squared = c * c;
        const double c_cubed = c_squared * c;

        const double numerator[4] = {
             b3 * c_cubed + b2 * c_squared + b1 * c,
            -3.0 * b3 * c_cubed - b2 * c_squared + b1 * c,
             3.0 * b3 * c_cubed - b2 * c_squared - b1 * c,
            -b3 * c_cubed + b2 * c_squared - b1 * c
        };
        const double denominator[4] = {
             a3 * c_cubed + a2 * c_squared + a1 * c + 1.0,
            -3.0 * a3 * c_cubed - a2 * c_squared + a1 * c + 3.0,
             3.0 * a3 * c_cubed - a2 * c_squared - a1 * c + 3.0,
            -a3 * c_cubed + a2 * c_squared - a1 * c + 1.0
        };

        const double normalisation = 1.0 / denominator[0];
        for (int i = 0; i < 4; ++i)
        {
            b_[i] = numerator[i] * normalisation;
            a_[i] = denominator[i] * normalisation;
        }
    }

    float FmvToneStack::process_sample(float input)
    {
        const double x = static_cast<double>(input);
        const double y = b_[0] * x + b_[1] * state_x_[0] + b_[2] * state_x_[1] + b_[3] * state_x_[2]
                       - a_[1] * state_y_[0] - a_[2] * state_y_[1] - a_[3] * state_y_[2];

        state_x_[2] = state_x_[1];
        state_x_[1] = state_x_[0];
        state_x_[0] = x;
        state_y_[2] = state_y_[1];
        state_y_[1] = state_y_[0];
        state_y_[0] = y;

        return static_cast<float>(y);
    }

    void FmvToneStack::reset()
    {
        for (int i = 0; i < 3; ++i)
        {
            state_x_[i] = 0.0;
            state_y_[i] = 0.0;
        }
    }
}
