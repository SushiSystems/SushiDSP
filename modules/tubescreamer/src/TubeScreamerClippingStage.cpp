/**************************************************************************/
/* TubeScreamerClippingStage.cpp                                          */
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

#include "SushiDSP/tubescreamer/TubeScreamerClippingStage.hpp"

#include <cmath>

#include "SushiDSP/math/NewtonRaphsonSolver.hpp"

namespace
{
    // Same bound DiodeClipperCircuit (D2) established, reused verbatim: the residual contains
    // exp(v / Vt), so an unbounded Newton overshoot would evaluate exp() past the float range,
    // produce inf, then nan, and the warm start would carry that forever. At exp(60) ~ 1.1e26 the
    // diode term still evaluates finitely in float, while 60 * Vt ~ 1.55 V sits far above any
    // physical silicon diode drop (~0.6-0.8 V), so the clamp bounds transient overshoot only and
    // never truncates a real solution.
    constexpr float kExponentLimit = 60.0f;
}

namespace SushiDSP
{
    TubeScreamerClippingStage::TubeScreamerClippingStage(
        float input_resistance_ohms, float feedback_resistance_ohms,
        float saturation_current_amps, float thermal_voltage_volts)
        : input_resistance_(input_resistance_ohms)
        , feedback_resistance_(feedback_resistance_ohms)
        , saturation_current_(saturation_current_amps)
        , thermal_voltage_(thermal_voltage_volts)
        , voltage_limit_(kExponentLimit * thermal_voltage_volts)
    {
    }

    float TubeScreamerClippingStage::process_sample(float input_voltage)
    {
        const float r1 = input_resistance_;
        const float rf = feedback_resistance_;
        const float is = saturation_current_;
        const float vt = thermal_voltage_;
        const float vin = input_voltage;

        // Virtual-ground KCL at the inverting input: Vin/R1 = -Vout/Rf - Id(Vout), with the
        // antiparallel diode pair contributing Id(v) = Is * (exp(v/Vt) - exp(-v/Vt)).
        // The two exponentials serve both the residual and its derivative; they are the whole
        // cost of the iteration.
        auto node_equation = [r1, rf, is, vt, vin](float v)
        {
            const float forward = std::exp(v / vt);
            const float reverse = std::exp(-v / vt);
            return NewtonRaphsonSolver::Evaluation{
                v / rf + is * (forward - reverse) + vin / r1,
                1.0f / rf + (is / vt) * (forward + reverse) };
        };

        const float voltage = NewtonRaphsonSolver::solve_joint(
            node_equation, previous_voltage_, 4, -voltage_limit_, voltage_limit_);

        previous_voltage_ = voltage;
        return voltage;
    }

    void TubeScreamerClippingStage::set_feedback_resistance(float feedback_resistance_ohms)
    {
        feedback_resistance_ = feedback_resistance_ohms;
    }

    void TubeScreamerClippingStage::reset()
    {
        previous_voltage_ = 0.0f;
    }
}
