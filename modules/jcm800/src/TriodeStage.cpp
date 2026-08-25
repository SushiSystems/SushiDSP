/**************************************************************************/
/* TriodeStage.cpp                                                        */
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

#include "SushiDSP/jcm800/TriodeStage.hpp"

#include <cmath>

#include "SushiDSP/math/NewtonRaphsonSolver.hpp"

namespace
{
    // Same bound DiodeClipperCircuit (D2) and TubeScreamerClippingStage (D3) established: the
    // residual contains exp(x / Vt), so an unbounded Newton overshoot would evaluate exp() past
    // the float range, produce inf, then nan, and the warm start would carry that forever. At
    // exp(60) ~ 1.1e26 the term still evaluates finitely in float, while 60 * Vt ~ 1.55 V sits far
    // above any physical grid-conduction drop, so the clamp bounds transient overshoot only.
    //
    // Unlike the diode-pair circuits, the bound is applied *around the cathode-bias voltage Vk*
    // rather than around 0, because Vk - not ground - is this stage's resting operating point.
    constexpr float kExponentLimit = 60.0f;
}

namespace SushiDSP
{
    TriodeStage::TriodeStage(
        float source_resistance_ohms, float grid_leak_resistance_ohms, float cathode_bias_volts,
        float gain, float saturation_current_amps, float thermal_voltage_volts)
        : source_resistance_(source_resistance_ohms)
        , grid_leak_resistance_(grid_leak_resistance_ohms)
        , cathode_bias_(cathode_bias_volts)
        , gain_(gain)
        , saturation_current_(saturation_current_amps)
        , thermal_voltage_(thermal_voltage_volts)
        , voltage_limit_(kExponentLimit * thermal_voltage_volts)
        , previous_grid_voltage_(cathode_bias_volts)
    {
    }

    float TriodeStage::process_sample(float input_voltage)
    {
        const float rs = source_resistance_;
        const float rg = grid_leak_resistance_;
        const float vk = cathode_bias_;
        const float is = saturation_current_;
        const float vt = thermal_voltage_;

        // The grid is driven through a coupling capacitor, so the source contributes signal only:
        // its DC level is set by the grid-leak return, and the grid node rests at Vk when the
        // input is silent. The source therefore appears as Vk + Vin behind Rs.
        const float source_voltage = vk + input_voltage;

        // KCL at the grid node, with all three branch currents taken as leaving the node:
        //   through Rs to the source:            (Vg - (Vk + Vin)) / Rs
        //   through the grid-leak Rg to Vk:      (Vg - Vk) / Rg
        //   into the grid itself, a one-way
        //   Shockley conduction path grid->cathode, only significant once Vg rises above Vk:
        //                                        Is * (exp((Vg - Vk) / Vt) - 1)
        // f(Vg) = 0 is the node equation; f'(Vg) differentiates it term by term. The exponential
        // term is what makes the positive half of the swing clip hard while the negative half,
        // where the term collapses to -Is, stays essentially linear - the asymmetry that gives a
        // driven triode its character.
        auto residual = [rs, rg, vk, is, vt, source_voltage](float vg)
        {
            return (vg - source_voltage) / rs + (vg - vk) / rg + is * (std::exp((vg - vk) / vt) - 1.0f);
        };
        auto derivative = [rs, rg, vk, is, vt](float vg)
        {
            return 1.0f / rs + 1.0f / rg + (is / vt) * std::exp((vg - vk) / vt);
        };

        const float grid_voltage = NewtonRaphsonSolver::solve(
            residual, derivative, previous_grid_voltage_, 4,
            vk - voltage_limit_, vk + voltage_limit_);
        previous_grid_voltage_ = grid_voltage;

        // Common-cathode plate output: the grid's excursion from its bias point, amplified and
        // inverted.
        return -gain_ * (grid_voltage - vk);
    }

    void TriodeStage::reset()
    {
        previous_grid_voltage_ = cathode_bias_;
    }
}
