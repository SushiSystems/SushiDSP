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

namespace SushiDSP
{
    /// @brief An EL34 push-pull half-channel: the same grid-conduction Newton-Raphson
    /// nonlinearity TriodeStage uses, followed by an explicit tanh plate-saturation waveshaper.
    /// An amplifier module instantiates one per push-pull half.
    class PentodeStage
    {
    public:
        /// @param source_resistance_ohms Effective source resistance feeding the grid.
        /// @param grid_leak_resistance_ohms The grid-leak resistor to the cathode-bias voltage.
        /// @param cathode_bias_volts The stage's DC operating point.
        /// @param gain The stage's inverting linear gain from grid to plate before saturation.
        /// @param saturation_current_amps Grid-conduction diode saturation current.
        /// @param thermal_voltage_volts Diode thermal voltage (~0.02585 V at room temperature).
        /// @param plate_saturation_drive Controls how hard the plate-current tanh saturates;
        /// smaller values saturate at a higher output ceiling (1 / plate_saturation_drive).
        /// Non-positive values disable the saturation and leave the plate output linear.
        PentodeStage(
            float source_resistance_ohms, float grid_leak_resistance_ohms, float cathode_bias_volts,
            float gain, float saturation_current_amps, float thermal_voltage_volts,
            float plate_saturation_drive);

        /// @brief Processes one sample through the stage.
        /// @param input_voltage The stage's input signal voltage for this sample, referenced to
        /// the stage's own bias point (the coupling capacitor ahead of the grid blocks DC, so a
        /// zero input leaves the grid resting at the cathode-bias voltage).
        /// @return The saturated, inverted plate output voltage. Always finite.
        float process_sample(float input_voltage);

        /// @brief Resets the Newton-Raphson warm-start state to the cathode bias point.
        void reset();

    private:
        float source_resistance_;
        float grid_leak_resistance_;
        float cathode_bias_;
        float gain_;
        float saturation_current_;
        float thermal_voltage_;
        float plate_saturation_drive_;
        float voltage_limit_;
        float cutoff_voltage_limit_;
        float previous_grid_voltage_;
    };
}
