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
    /// @brief How far a stage's plate can swing from its idle point before it saturates.
    struct PlateSwing
    {
        /// @brief B+ minus the idle plate voltage; the cutoff side of the swing.
        float headroom_to_supply_volts;

        /// @brief Idle plate voltage minus the plate at Vgk = 0 on the load line; the
        /// conduction side of the swing.
        float headroom_to_bottom_volts;
    };

    /// @brief A common-cathode triode gain stage: grid-conduction diode nonlinearity solved by
    /// Newton-Raphson, followed by a fixed inverting linear plate gain and, when the stage is
    /// given a PlateSwing, a two-sided tanh plate ceiling. On the PlateSwing path the grid
    /// conduction diode is referenced to the cathode rather than to the bias point, so
    /// conduction begins at Vgk = 0; see docs/superpowers/specs/
    /// 2026-09-07-triode-plate-swing-design.md section 3. Used by the JCM800 and by every
    /// cascaded-triode amplifier module.
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

        /// @brief Builds a stage whose plate is bounded by its swing and whose grid conducts at
        /// Vgk = 0, referenced to the cathode.
        /// @param source_resistance_ohms Effective source resistance feeding the grid.
        /// @param grid_leak_resistance_ohms The grid-leak resistor to the cathode-bias voltage.
        /// @param cathode_bias_volts The stage's DC operating point (self-bias voltage).
        /// @param gain The common-cathode stage's inverting linear gain from grid to plate.
        /// @param saturation_current_amps Grid-conduction diode saturation current.
        /// @param thermal_voltage_volts Diode thermal voltage (~0.02585 V at room temperature).
        /// @param plate_swing The two headroom figures that bound the plate output.
        /// @pre cathode_bias_volts < 0, so the grid rests below the cathode.
        /// @pre plate_swing.headroom_to_supply_volts > 0 and
        /// plate_swing.headroom_to_bottom_volts > 0; a zero headroom divides by zero at p = 0.
        TriodeStage(
            float source_resistance_ohms, float grid_leak_resistance_ohms, float cathode_bias_volts,
            float gain, float saturation_current_amps, float thermal_voltage_volts,
            const PlateSwing& plate_swing);

        /// @brief Processes one sample through the stage.
        /// @param input_voltage The stage's input signal voltage for this sample, referenced to
        /// the stage's own bias point (the coupling capacitor ahead of the grid blocks DC, so a
        /// zero input leaves the grid resting at the cathode-bias voltage).
        /// @return The inverted, grid-conduction-clipped plate output voltage, bounded by the
        /// plate swing when the stage was given one. Always finite.
        float process_sample(float input_voltage);

        /// @brief Returns the largest |Vg - Vk| the grid has reached since the last reset().
        float peak_grid_excursion_volts() const { return peak_grid_excursion_; }

        /// @brief Resets the Newton-Raphson warm-start state to the cathode bias point and
        /// clears the peak grid excursion.
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
        float conduction_reference_;
        PlateSwing plate_swing_{};
        bool has_plate_swing_ = false;
        float peak_grid_excursion_ = 0.0f;
    };
}
