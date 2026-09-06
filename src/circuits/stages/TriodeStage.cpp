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

#include "SushiDSP/circuits/stages/TriodeStage.hpp"

#include <algorithm>
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
    // Unlike the diode-pair circuits, neither half of the bound sits around 0. The lower half is
    // applied around the cathode-bias voltage Vk on both paths, because Vk - not ground - is this
    // stage's resting operating point. The upper half follows the conduction reference the diode
    // uses, so it sits at Vk on the six-argument path and at the cathode, 0, on the seven-argument
    // one; the two references are spec section 3's ruling.
    //
    // The bound is deliberately asymmetric, and only the upper half of it is a numerical guard.
    // Below Vk the exponential term collapses to -Is, so the residual is linear, Newton converges
    // exactly in one step, and no overshoot is possible - there is nothing there for a clamp to
    // protect against. What a symmetric bound does on that side is silently hard-clip an otherwise
    // linear answer, which is why Jcm800's interstage levels previously had to be held so low that
    // the cascade barely distorted (see Jcm800.cpp's interstage gain-staging comment and the Task
    // 9 voicing ruling). The lower bound is therefore set at 160 * Vt ~ 4.14 V below Vk, which is
    // where a 12AX7's plate swing hits the ~250 V B+ rail at this stage's gain of 60 - so it reads
    // as the stage's cutoff rather than as an arbitrary numerical fence, it sits above the 2.81 V
    // worst case the driven chain reaches at a 1.0 input, and a genuine runaway is still caught
    // rather than integrated forever by the warm start.
    constexpr float kExponentLimit = 60.0f;
    constexpr float kNegativeExponentLimit = 160.0f;
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
        , cutoff_voltage_limit_(kNegativeExponentLimit * thermal_voltage_volts)
        , previous_grid_voltage_(cathode_bias_volts)
        , conduction_reference_(cathode_bias_volts)
    {
    }

    TriodeStage::TriodeStage(
        float source_resistance_ohms, float grid_leak_resistance_ohms, float cathode_bias_volts,
        float gain, float saturation_current_amps, float thermal_voltage_volts,
        const PlateSwing& plate_swing)
        : TriodeStage(
              source_resistance_ohms, grid_leak_resistance_ohms, cathode_bias_volts, gain,
              saturation_current_amps, thermal_voltage_volts)
    {
        plate_swing_ = plate_swing;
        has_plate_swing_ = true;
        conduction_reference_ = 0.0f;
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
        //   Shockley conduction path grid->cathode, only significant once Vg rises above the
        //   conduction reference Vref:          Is * (exp((Vg - Vref) / Vt) - 1)
        // f(Vg) = 0 is the node equation; f'(Vg) differentiates it term by term. The exponential
        // term is what makes the positive half of the swing clip hard while the negative half,
        // where the term collapses to -Is, stays essentially linear - the asymmetry that gives a
        // driven triode its character.
        // One exp() serves both: it is the whole cost of the iteration.
        const float vref = conduction_reference_;

        auto node_equation = [rs, rg, vk, vref, is, vt, source_voltage](float vg)
        {
            const float conduction = std::exp((vg - vref) / vt);
            return NewtonRaphsonSolver::Evaluation{
                (vg - source_voltage) / rs + (vg - vk) / rg + is * (conduction - 1.0f),
                1.0f / rs + 1.0f / rg + (is / vt) * conduction };
        };

        // The upper bound tracks the conduction reference, the lower one the bias point (spec 3).
        const float grid_voltage = NewtonRaphsonSolver::solve_joint(
            node_equation, previous_grid_voltage_, 4,
            vk - cutoff_voltage_limit_, vref + voltage_limit_);
        previous_grid_voltage_ = grid_voltage;

        const float grid_excursion = std::fabs(grid_voltage - vk);
        peak_grid_excursion_ = std::max(peak_grid_excursion_, grid_excursion);

        // Common-cathode plate output: the grid's excursion from its bias point, amplified and
        // inverted.
        const float linear_plate = -gain_ * (grid_voltage - vk);

        if (!has_plate_swing_)
        {
            return linear_plate;
        }

        // Each side is bounded by its own headroom, so the output stays inside the swing (spec 3).
        if (linear_plate > 0.0f)
        {
            const float up = plate_swing_.headroom_to_supply_volts;
            return up * std::tanh(linear_plate / up);
        }

        const float down = plate_swing_.headroom_to_bottom_volts;
        return -down * std::tanh(-linear_plate / down);
    }

    void TriodeStage::reset()
    {
        previous_grid_voltage_ = cathode_bias_;
        peak_grid_excursion_ = 0.0f;
    }
}
