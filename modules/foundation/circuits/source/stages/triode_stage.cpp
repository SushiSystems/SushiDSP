/**************************************************************************/
/* triode_stage.cpp                                                       */
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
 * @file triode_stage.cpp
 * @brief Implements the grid solve and plate output of the triode gain stage.
 * @author Mustafa Garip
 */

#include <SushiDSP/foundation/circuits/stages/triode_stage.hpp>

#include <algorithm>
#include <cmath>

#include <SushiDSP/foundation/circuits/models/soft_transition.hpp>
#include <SushiDSP/foundation/math/newton_raphson_solver.hpp>

namespace
{
    /// @brief The tanh argument past which the plate ceiling reads 1.0 in float; the solver's
    /// grid bounds sit where the plate can no longer move
    /// (the grid knee antialiasing design spec in the version history section 3).
    constexpr float K_PLATE_REACH = 10.0f;

    /// @brief Newton iterations per sample from the previous sample's root.
    constexpr std::uint32_t K_NEWTON_ITERATIONS = 4;
} // namespace

namespace SushiDSP
{
    TriodeStage::TriodeStage(float source_resistance_ohms, float grid_leak_resistance_ohms,
                             float cathode_bias_volts, float gain,
                             const GridConductionLaw& grid_conduction,
                             const PlateSwing& plate_swing)
        : source_resistance_(source_resistance_ohms),
          grid_leak_resistance_(grid_leak_resistance_ohms), cathode_bias_(cathode_bias_volts),
          grid_conduction_(grid_conduction),
          cutoff_grid_limit_(cathode_bias_volts -
                             K_PLATE_REACH * plate_swing.headroom_to_supply_volts / gain),
          conduction_grid_limit_(cathode_bias_volts +
                                 K_PLATE_REACH * plate_swing.headroom_to_bottom_volts / gain),
          previous_grid_voltage_(cathode_bias_volts),
          antialiaser_(source_resistance_ohms, grid_leak_resistance_ohms, cathode_bias_volts, gain,
                       grid_conduction, plate_swing)
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

        // KCL at the grid, every branch current leaving the node (modules/foundation/circuits/README.md).
        auto node_equation = [rs, rg, vk, source_voltage, scale, threshold, transition](float vg)
        {
            double softplus = 0.0;
            double logistic = 0.0;
            models::softplus_and_logistic((static_cast<double>(vg) - threshold) / transition,
                                          softplus, logistic);
            return NewtonRaphsonSolver::Evaluation{
                (vg - source_voltage) / rs + (vg - vk) / rg + static_cast<float>(scale * softplus),
                1.0f / rs + 1.0f / rg + static_cast<float>(scale / transition * logistic)};
        };

        // The residual is convex and increasing in Vg (modules/foundation/circuits/README.md).
        float grid_voltage = previous_grid_voltage_;
        if (!root_stays_on_bound(grid_voltage, source_voltage))
        {
            grid_voltage = NewtonRaphsonSolver::solve_joint(node_equation, previous_grid_voltage_,
                                                            K_NEWTON_ITERATIONS, cutoff_grid_limit_,
                                                            conduction_grid_limit_);
        }
        previous_grid_voltage_ = grid_voltage;

        const float grid_excursion = std::fabs(grid_voltage - vk);
        peak_grid_excursion_ = std::max(peak_grid_excursion_, grid_excursion);

        return antialiaser_.process(grid_voltage - vk);
    }

    bool TriodeStage::root_stays_on_bound(float grid_voltage, float source_voltage) const
    {
        // A Newton step from a bound stays on it when the residual there points past it (modules/foundation/circuits/README.md).
        const float linear_residual = (grid_voltage - source_voltage) / source_resistance_ +
                                      (grid_voltage - cathode_bias_) / grid_leak_resistance_;
        if (grid_voltage == cutoff_grid_limit_)
        {
            return linear_residual >= 0.0f;
        }
        if (grid_voltage == conduction_grid_limit_)
        {
            const double argument =
                (static_cast<double>(grid_voltage) - grid_conduction_.threshold_volts) /
                grid_conduction_.transition_volts;
            return argument >= 30.0 &&
                   linear_residual +
                           static_cast<float>(static_cast<double>(grid_conduction_.scale_amps) *
                                              argument) <=
                       0.0f;
        }
        return false;
    }

    void TriodeStage::reset()
    {
        previous_grid_voltage_ = cathode_bias_;
        peak_grid_excursion_ = 0.0f;
        antialiaser_.reset();
    }
} // namespace SushiDSP
