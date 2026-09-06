/**************************************************************************/
/* PentodeStage.cpp                                                       */
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

#include "SushiDSP/circuits/stages/PentodeStage.hpp"

#include <cmath>

#include "SushiDSP/math/NewtonRaphsonSolver.hpp"

namespace
{
    // Identical to TriodeStage's bound, and for the same reason: the residual contains
    // exp(x / Vt), so an unbounded Newton overshoot would evaluate exp() past the float range,
    // produce inf, then nan, and the warm start would carry that forever. At exp(60) ~ 1.1e26 the
    // term still evaluates finitely in float, while 60 * Vt ~ 1.55 V sits far above any physical
    // grid-conduction drop, so the clamp bounds transient overshoot only. The bound is applied
    // *around the cathode-bias voltage Vk*, not around 0, because Vk is this stage's resting point.
    //
    // Asymmetric for the same reason TriodeStage's is - see the longer ruling at its definition
    // site. Below Vk the exponential collapses to -Is and the residual is linear, so the lower
    // clamp guards nothing and only hard-clips a would-be-linear answer; it is opened to
    // 160 * Vt ~ 4.14 V so the driven chain reaches its own grid conduction and the pentode's
    // plate saturation, rather than a solver artifact, on the way to clipping. This stage's own
    // measured worst case is under 0.5 V, so for the EL34s the widening is headroom, not a
    // behavior change - the tanh plate saturation was already the binding limit here.
    constexpr float kExponentLimit = 60.0f;
    constexpr float kNegativeExponentLimit = 160.0f;
}

namespace SushiDSP
{
    PentodeStage::PentodeStage(
        float source_resistance_ohms, float grid_leak_resistance_ohms, float cathode_bias_volts,
        float gain, float saturation_current_amps, float thermal_voltage_volts,
        float plate_saturation_drive)
        : source_resistance_(source_resistance_ohms)
        , grid_leak_resistance_(grid_leak_resistance_ohms)
        , cathode_bias_(cathode_bias_volts)
        , gain_(gain)
        , saturation_current_(saturation_current_amps)
        , thermal_voltage_(thermal_voltage_volts)
        , plate_saturation_drive_(plate_saturation_drive)
        , voltage_limit_(kExponentLimit * thermal_voltage_volts)
        , cutoff_voltage_limit_(kNegativeExponentLimit * thermal_voltage_volts)
        , previous_grid_voltage_(cathode_bias_volts)
    {
    }

    PentodeStage::PentodeStage(const PowerTubePreset& preset)
        : PentodeStage(
            preset.source_resistance_ohms, preset.grid_leak_resistance_ohms,
            preset.cathode_bias_volts, preset.gain, preset.saturation_current_amps,
            preset.thermal_voltage_volts, preset.plate_saturation_drive)
    {
    }

    float PentodeStage::process_sample(float input_voltage)
    {
        const float rs = source_resistance_;
        const float rg = grid_leak_resistance_;
        const float vk = cathode_bias_;
        const float is = saturation_current_;
        const float vt = thermal_voltage_;

        // The control grid is driven through a coupling capacitor, so the source contributes
        // signal only: its DC level is set by the grid-leak return, and the grid node rests at Vk
        // when the input is silent. The source therefore appears as Vk + Vin behind Rs. Writing it
        // this way keeps the node equation bias-invariant; referencing the source term to ground
        // instead would leave the stage clipping at silence for any non-zero Vk (and an EL34's Vk
        // is large - around -20 V here).
        const float source_voltage = vk + input_voltage;

        // KCL at the grid node, with all three branch currents taken as leaving the node:
        //   through Rs to the source:            (Vg - (Vk + Vin)) / Rs
        //   through the grid-leak Rg to Vk:      (Vg - Vk) / Rg
        //   into the grid itself, a one-way
        //   Shockley conduction path grid->cathode, only significant once Vg rises above Vk:
        //                                        Is * (exp((Vg - Vk) / Vt) - 1)
        // f(Vg) = 0 is the node equation; f'(Vg) differentiates it term by term. A pentode's
        // control grid conducts exactly the way a triode's does, so this is TriodeStage's equation
        // verbatim - the pentode character comes from the plate saturation below, not from here.
        // One exp() serves both: it is the whole cost of the iteration.
        auto node_equation = [rs, rg, vk, is, vt, source_voltage](float vg)
        {
            const float conduction = std::exp((vg - vk) / vt);
            return NewtonRaphsonSolver::Evaluation{
                (vg - source_voltage) / rs + (vg - vk) / rg + is * (conduction - 1.0f),
                1.0f / rs + 1.0f / rg + (is / vt) * conduction };
        };

        const float grid_voltage = NewtonRaphsonSolver::solve_joint(
            node_equation, previous_grid_voltage_, 4,
            vk - cutoff_voltage_limit_, vk + voltage_limit_);
        previous_grid_voltage_ = grid_voltage;

        // Inverting plate output: the grid's excursion from its bias point, amplified.
        const float linear_plate = -gain_ * (grid_voltage - vk);

        // Plate-current saturation. Unlike the grid conduction above this needs no iterative
        // solve: it is an explicit function of the already-solved grid voltage, so it is a single
        // tanh evaluation. tanh is bounded by 1, so the output is bounded by
        // 1 / plate_saturation_drive and can never be inf or nan for any finite grid solution.
        if (plate_saturation_drive_ > 0.0f)
        {
            return std::tanh(linear_plate * plate_saturation_drive_) / plate_saturation_drive_;
        }

        // A non-positive drive is the degenerate "no saturation" case; tanh(x)/d tends to x as
        // d tends to 0, so the linear plate output is the consistent answer (and avoids a divide
        // by zero).
        return linear_plate;
    }

    void PentodeStage::reset()
    {
        previous_grid_voltage_ = cathode_bias_;
    }
}
