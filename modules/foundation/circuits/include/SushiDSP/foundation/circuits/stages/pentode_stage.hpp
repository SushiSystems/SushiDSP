/**************************************************************************/
/* pentode_stage.hpp                                                      */
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
 * @file pentode_stage.hpp
 * @brief Declares one push-pull half of a power stage.
 * @author Mustafa Garip
 */

#pragma once

#include <SushiDSP/foundation/math/newton_raphson_solver.hpp>
#include <SushiDSP/foundation/circuits/stages/antiderivative_antialiaser.hpp>
#include <SushiDSP/foundation/circuits/stages/grid_conduction_law.hpp>
#include <SushiDSP/foundation/circuits/stages/plate_swing.hpp>
#include <SushiDSP/foundation/circuits/stages/power_tube_presets.hpp>

namespace SushiDSP
{
    /// @brief One push-pull half of a power stage: the grid node solved by Newton-Raphson against
    /// a Norman grid-conduction law referenced to the cathode, an inverting gain from grid to
    /// plate, and a two-sided tanh plate ceiling from the tube's PlateSwing, all in real volts,
    /// the composite transfer antialiased by an AntiderivativeAntialiaser. Specified by
    /// the grid knee antialiasing design spec in the version history sections 5 and 15.
    class PentodeStage
    {
        public:
            /// @brief Builds a stage whose plate is bounded by its swing and whose grid conducts on
            /// the given law, referenced to the cathode.
            /// @param source_resistance_ohms Effective source resistance feeding the grid.
            /// @param grid_leak_resistance_ohms The grid-leak resistor to the bias supply.
            /// @param cathode_bias_volts The grid's DC voltage against the cathode at rest.
            /// @param gain The inverting small-signal gain from grid to plate.
            /// @pre cathode_bias_volts < 0, so the grid rests below the cathode.
            /// @pre gain > 0, and both plate_swing headrooms > 0.
            PentodeStage(float source_resistance_ohms, float grid_leak_resistance_ohms,
                         float cathode_bias_volts, float gain,
                         const GridConductionLaw& grid_conduction, const PlateSwing& plate_swing);

            /// @brief Builds the stage from a named preset; identical to the six-argument constructor.
            explicit PentodeStage(const PowerTubePreset& preset);

            /// @brief Processes one sample through the stage.
            /// @param input_voltage The grid signal in volts, referenced to the bias point; a zero
            /// input leaves the grid at the cathode-bias voltage.
            /// @return The inverted plate output voltage, bounded by the plate swing and antialiased
            /// over the interval from the previous sample. Always finite.
            float process_sample(float input_voltage);

            /// @brief Adds the Newton solves run since the last call to @p into and clears them.
            /// Audio thread; an amplifier calls it once per block for each stage it owns.
            void take_newton_tally(NewtonRaphsonSolver::Tally& into) noexcept;

            /// @brief Resets the Newton-Raphson warm start to the bias point and drops the
            /// antialiaser's previous sample.
            void reset();

        private:
            /// @brief Reports whether a warm start sitting on one of the grid bounds is already the
            /// solve's result, which the residual's sign there decides without the exponential.
            /// @param grid_voltage The warm start, the previous sample's root.
            /// @param source_voltage The cathode bias plus this sample's input.
            bool root_stays_on_bound(float grid_voltage, float source_voltage) const;

            /// @brief Effective source resistance feeding the grid, in ohms.
            float source_resistance_;

            /// @brief The grid-leak resistance to the bias supply, in ohms.
            float grid_leak_resistance_;

            /// @brief The grid's resting voltage relative to the cathode, in volts.
            float cathode_bias_;

            /// @brief The grid-current law the node equation carries.
            GridConductionLaw grid_conduction_;

            /// @brief The grid voltage below which the plate tanh has saturated toward the supply.
            float cutoff_grid_limit_;

            /// @brief The grid voltage above which the plate tanh has saturated toward the bottom.
            float conduction_grid_limit_;

            /// @brief The previous sample's grid voltage, the solver's warm start.
            float previous_grid_voltage_;

            /// @brief The antialiaser of the composite grid-and-plate transfer.
            AntiderivativeAntialiaser antialiaser_;

            /// @brief The Newton solves run since the last take_newton_tally().
            NewtonRaphsonSolver::Tally newton_tally_;
    };
} // namespace SushiDSP
