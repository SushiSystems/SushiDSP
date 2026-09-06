/**************************************************************************/
/* PowerTubePresets.hpp                                                   */
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
    /// @brief The constructor constants one PentodeStage takes for a named power tube.
    struct PowerTubePreset
    {
        /// @brief The Thevenin source resistance feeding the grid, driver output impedance plus
        /// grid stopper, as PentodeStage.hpp defines it.
        float source_resistance_ohms;
        /// @brief The grid-leak resistor from grid to the cathode-bias voltage.
        float grid_leak_resistance_ohms;
        /// @brief The solver's resting grid-circuit potential; the output is the same function of
        /// the input for any value of it.
        float cathode_bias_volts;
        /// @brief The inverting linear gain from grid to plate before saturation.
        float gain;
        /// @brief The saturation current of the grid-conduction diode.
        float saturation_current_amps;
        /// @brief The thermal voltage of that diode, 0.02585 V at room temperature.
        float thermal_voltage_volts;
        /// @brief How hard the plate-current tanh saturates; the output ceiling is its reciprocal.
        float plate_saturation_drive;
    };

    /// @brief EL34 as the JCM800 2203 runs it; the values Jcm800.cpp used before DA0, verbatim.
    /// These are modelling constants calibrated against the 2203's golden files, not the 2203's
    /// schematic values. No 2203 reference operating point is stated anywhere in the JCM800
    /// design spec; the constants were fitted to the goldens without one.
    inline constexpr PowerTubePreset kEl34{ 10000.0f, 220000.0f, -20.0f, 8.0f, 1e-9f, 0.02585f, 0.05f };
}
