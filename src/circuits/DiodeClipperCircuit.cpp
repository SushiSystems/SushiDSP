/**************************************************************************/
/* DiodeClipperCircuit.cpp                                                */
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

#include "SushiDSP/circuits/DiodeClipperCircuit.hpp"

#include <cmath>

#include "SushiDSP/math/NewtonRaphsonSolver.hpp"

namespace
{
    // The Newton iterate is clamped to +/- kExponentLimit * Vt. Rationale: the residual contains
    // exp(v / Vt), and a single Newton overshoot (the first step of a cold start at Vs = 5 V lands
    // at v = Vs) would evaluate exp() far past the float range, producing inf, then nan on the next
    // division — which the warm start would then carry into every subsequent sample forever. At
    // exp(60) ~ 1.1e26 the diode term still evaluates finitely in float for any realistic Is * R,
    // while 60 * Vt ~ 1.55 V sits far above any physical silicon diode drop (~0.6-0.8 V), so the
    // clamp never truncates a real solution — it only bounds the transient overshoot.
    constexpr float kExponentLimit = 60.0f;
}

namespace SushiDSP
{
    DiodeClipperCircuit::DiodeClipperCircuit(
        float source_resistance_ohms, float saturation_current_amps, float thermal_voltage_volts)
        : source_resistance_(source_resistance_ohms)
        , saturation_current_(saturation_current_amps)
        , thermal_voltage_(thermal_voltage_volts)
        , voltage_limit_(kExponentLimit * thermal_voltage_volts)
    {
    }

    float DiodeClipperCircuit::process_sample(float source_voltage)
    {
        const float r = source_resistance_;
        const float is = saturation_current_;
        const float vt = thermal_voltage_;
        const float a = source_voltage;

        auto residual = [r, is, vt, a](float v)
        {
            return r * is * (std::exp(v / vt) - std::exp(-v / vt)) + v - a;
        };
        auto derivative = [r, is, vt](float v)
        {
            return r * is / vt * (std::exp(v / vt) + std::exp(-v / vt)) + 1.0f;
        };

        const float voltage = NewtonRaphsonSolver::solve(
            residual, derivative, previous_voltage_, 4, -voltage_limit_, voltage_limit_);

        previous_voltage_ = voltage;
        return voltage;
    }

    void DiodeClipperCircuit::reset()
    {
        previous_voltage_ = 0.0f;
    }
}
