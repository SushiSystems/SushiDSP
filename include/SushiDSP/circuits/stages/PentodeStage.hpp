/**************************************************************************/
/* PentodeStage.hpp                                                       */
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

#pragma once

#include "SushiDSP/circuits/stages/AntiderivativeAntialiaser.hpp"
#include "SushiDSP/circuits/stages/GridConductionLaw.hpp"
#include "SushiDSP/circuits/stages/PlateSwing.hpp"
#include "SushiDSP/circuits/stages/PowerTubePresets.hpp"

namespace SushiDSP
{
    /// @brief One push-pull half of a power stage: the grid node solved by Newton-Raphson against
    /// a Norman grid-conduction law referenced to the cathode, an inverting gain from grid to
    /// plate, and a two-sided tanh plate ceiling from the tube's PlateSwing, all in real volts,
    /// the composite transfer antialiased by an AntiderivativeAntialiaser. Specified by
    /// docs/superpowers/specs/2026-09-07-grid-knee-antialiasing-design.md sections 5 and 15.
    class PentodeStage
    {
    public:
        /// @brief Builds a stage whose plate is bounded by its swing and whose grid conducts on
        /// the given law, referenced to the cathode.
        /// @param source_resistance_ohms Effective source resistance feeding the grid.
        /// @param grid_leak_resistance_ohms The grid-leak resistor to the bias supply.
        /// @param cathode_bias_volts The grid's DC voltage against the cathode at rest.
        /// @param gain The inverting small-signal gain from grid to plate.
        /// @param grid_conduction The grid-current law's three constants.
        /// @param plate_swing The two headroom figures that bound the plate output.
        /// @pre cathode_bias_volts < 0, so the grid rests below the cathode.
        /// @pre gain > 0, and both plate_swing headrooms > 0.
        PentodeStage(
            float source_resistance_ohms, float grid_leak_resistance_ohms, float cathode_bias_volts,
            float gain, const GridConductionLaw& grid_conduction, const PlateSwing& plate_swing);

        /// @brief Builds the stage from a named preset; identical to the six-argument constructor.
        explicit PentodeStage(const PowerTubePreset& preset);

        /// @brief Processes one sample through the stage.
        /// @param input_voltage The grid signal in volts, referenced to the bias point; a zero
        /// input leaves the grid at the cathode-bias voltage.
        /// @return The inverted plate output voltage, bounded by the plate swing and antialiased
        /// over the interval from the previous sample. Always finite.
        float process_sample(float input_voltage);

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
    };
}
