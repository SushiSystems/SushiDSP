/****************************************************************/
/* PentodeStage.cpp                                             */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file PentodeStage.cpp
 * @brief Implements the grid solve and plate output of one power-stage half.
 * @author Mustafa Garip
 */

#include "SushiDSP/circuits/stages/PentodeStage.hpp"

#include <cmath>

#include "SushiDSP/circuits/models/SoftTransition.hpp"
#include "SushiDSP/math/NewtonRaphsonSolver.hpp"

namespace
{
    /// @brief The tanh argument past which the plate ceiling reads 1.0 in float; the solver's
    /// grid bounds sit where the plate can no longer move
    /// (docs/archive/superpowers/specs/2026-09-07-grid-knee-antialiasing-design.md section 5).
    constexpr float kPlateReach = 10.0f;

    /// @brief Newton iterations per sample from the previous sample's root.
    constexpr std::uint32_t kNewtonIterations = 4;
}

namespace SushiDSP
{
    PentodeStage::PentodeStage(
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

    PentodeStage::PentodeStage(const PowerTubePreset& preset)
        : PentodeStage(
            preset.source_resistance_ohms, preset.grid_leak_resistance_ohms,
            preset.cathode_bias_volts, preset.gain, preset.grid_conduction, preset.plate_swing)
    {
    }

    float PentodeStage::process_sample(float input_voltage)
    {
        const float rs = source_resistance_;
        const float rg = grid_leak_resistance_;
        const float vk = cathode_bias_;
        const double scale = grid_conduction_.scale_amps;
        const double threshold = grid_conduction_.threshold_volts;
        const double transition = grid_conduction_.transition_volts;

        // The coupling capacitor passes signal only, so the source sits at Vk + Vin behind Rs.
        const float source_voltage = vk + input_voltage;

        // KCL at the grid, every branch current leaving the node (src/README.md).
        auto node_equation = [rs, rg, vk, source_voltage, scale, threshold, transition](float vg)
        {
            double softplus = 0.0;
            double logistic = 0.0;
            models::softplus_and_logistic((static_cast<double>(vg) - threshold) / transition, softplus, logistic);
            return NewtonRaphsonSolver::Evaluation{
                (vg - source_voltage) / rs + (vg - vk) / rg + static_cast<float>(scale * softplus),
                1.0f / rs + 1.0f / rg + static_cast<float>(scale / transition * logistic) };
        };

        // The residual is convex and increasing in Vg (src/README.md).
        float grid_voltage = previous_grid_voltage_;
        if (!root_stays_on_bound(grid_voltage, source_voltage))
        {
            grid_voltage = NewtonRaphsonSolver::solve_joint(
                node_equation, previous_grid_voltage_, kNewtonIterations,
                cutoff_grid_limit_, conduction_grid_limit_);
        }
        previous_grid_voltage_ = grid_voltage;

        return antialiaser_.process(grid_voltage - vk);
    }

    bool PentodeStage::root_stays_on_bound(float grid_voltage, float source_voltage) const
    {
        // A Newton step from a bound stays on it when the residual there points past it (src/README.md).
        const float linear_residual = (grid_voltage - source_voltage) / source_resistance_
                                      + (grid_voltage - cathode_bias_) / grid_leak_resistance_;
        if (grid_voltage == cutoff_grid_limit_)
        {
            return linear_residual >= 0.0f;
        }
        if (grid_voltage == conduction_grid_limit_)
        {
            const double argument = (static_cast<double>(grid_voltage) - grid_conduction_.threshold_volts)
                                    / grid_conduction_.transition_volts;
            return argument >= 30.0
                   && linear_residual + static_cast<float>(static_cast<double>(grid_conduction_.scale_amps) * argument) <= 0.0f;
        }
        return false;
    }

    void PentodeStage::reset()
    {
        previous_grid_voltage_ = cathode_bias_;
        antialiaser_.reset();
    }
}
