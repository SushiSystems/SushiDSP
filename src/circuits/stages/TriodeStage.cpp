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

#include "SushiDSP/circuits/models/SoftTransition.hpp"
#include "SushiDSP/math/NewtonRaphsonSolver.hpp"

namespace
{
    /// @brief The tanh argument past which the plate ceiling reads 1.0 in float; the solver's
    /// grid bounds sit where the plate can no longer move
    /// (docs/superpowers/specs/2026-09-07-grid-knee-antialiasing-design.md section 3).
    constexpr float kPlateReach = 10.0f;

    /// @brief Newton iterations per sample from the previous sample's root.
    constexpr std::uint32_t kNewtonIterations = 4;
}

namespace SushiDSP
{
    TriodeStage::TriodeStage(
        float source_resistance_ohms, float grid_leak_resistance_ohms, float cathode_bias_volts,
        float gain, const GridConductionLaw& grid_conduction, const PlateSwing& plate_swing)
        : source_resistance_(source_resistance_ohms)
        , grid_leak_resistance_(grid_leak_resistance_ohms)
        , cathode_bias_(cathode_bias_volts)
        , grid_conduction_(grid_conduction)
        , cutoff_grid_limit_(cathode_bias_volts - kPlateReach * plate_swing.headroom_to_supply_volts / gain)
        , conduction_grid_limit_(cathode_bias_volts + kPlateReach * plate_swing.headroom_to_bottom_volts / gain)
        , previous_grid_voltage_(cathode_bias_volts)
        , antialiaser_(source_resistance_ohms, grid_leak_resistance_ohms, cathode_bias_volts, gain, grid_conduction, plate_swing)
    {
    }

    float TriodeStage::process_sample(float input_voltage)
    {
        const float rs = source_resistance_;
        const float rg = grid_leak_resistance_;
        const float vk = cathode_bias_;
        const double scale = grid_conduction_.scale_amps;
        const double threshold = grid_conduction_.threshold_volts;
        const double transition = grid_conduction_.transition_volts;

        // The coupling capacitor passes signal only, so the source sits at Vk + Vin behind Rs.
        const float source_voltage = vk + input_voltage;

        // KCL at the grid, every branch current leaving the node: (Vg - Vk - Vin) / Rs +
        // (Vg - Vk) / Rg + IG0 softplus((Vg - VGAMMA) / VTG) = 0, Vg referenced to the cathode.
        auto node_equation = [rs, rg, vk, source_voltage, scale, threshold, transition](float vg)
        {
            double softplus = 0.0;
            double logistic = 0.0;
            models::softplus_and_logistic((static_cast<double>(vg) - threshold) / transition, softplus, logistic);
            return NewtonRaphsonSolver::Evaluation{
                (vg - source_voltage) / rs + (vg - vk) / rg + static_cast<float>(scale * softplus),
                1.0f / rs + 1.0f / rg + static_cast<float>(scale / transition * logistic) };
        };

        // The residual is convex and increasing in Vg, so Newton converges from either side;
        // the bounds mark where the plate ceiling has already saturated.
        const float grid_voltage = NewtonRaphsonSolver::solve_joint(
            node_equation, previous_grid_voltage_, kNewtonIterations,
            cutoff_grid_limit_, conduction_grid_limit_);
        previous_grid_voltage_ = grid_voltage;

        const float grid_excursion = std::fabs(grid_voltage - vk);
        peak_grid_excursion_ = std::max(peak_grid_excursion_, grid_excursion);

        return antialiaser_.process(grid_voltage - vk);
    }

    void TriodeStage::reset()
    {
        previous_grid_voltage_ = cathode_bias_;
        peak_grid_excursion_ = 0.0f;
        antialiaser_.reset();
    }
}
