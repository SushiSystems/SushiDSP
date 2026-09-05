/**************************************************************************/
/* TriodeStage.hpp                                                        */
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
    /// @brief A common-cathode triode gain stage: grid-conduction diode nonlinearity solved by
    /// Newton-Raphson, followed by a fixed inverting linear plate gain. Used by the JCM800 and
    /// by every cascaded-triode amplifier module.
    class TriodeStage
    {
    public:
        /// @param source_resistance_ohms Effective source resistance feeding the grid.
        /// @param grid_leak_resistance_ohms The grid-leak resistor to the cathode-bias voltage.
        /// @param cathode_bias_volts The stage's DC operating point (self-bias voltage).
        /// @param gain The common-cathode stage's inverting linear gain from grid to plate.
        /// @param saturation_current_amps Grid-conduction diode saturation current.
        /// @param thermal_voltage_volts Diode thermal voltage (~0.02585 V at room temperature).
        TriodeStage(
            float source_resistance_ohms, float grid_leak_resistance_ohms, float cathode_bias_volts,
            float gain, float saturation_current_amps, float thermal_voltage_volts);

        /// @brief Processes one sample through the stage.
        /// @param input_voltage The stage's input signal voltage for this sample, referenced to
        /// the stage's own bias point (the coupling capacitor ahead of the grid blocks DC, so a
        /// zero input leaves the grid resting at the cathode-bias voltage).
        /// @return The inverted, grid-conduction-clipped plate output voltage. Always finite.
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
        float voltage_limit_;
        float cutoff_voltage_limit_;
        float previous_grid_voltage_;
    };
}
