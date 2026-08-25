/**************************************************************************/
/* TubeScreamerClippingStage.hpp                                          */
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
    /// @brief The TS-family inverting op-amp clipping stage: virtual-ground KCL against an
    /// antiparallel diode pair in the feedback path, solved per sample by Newton-Raphson.
    class TubeScreamerClippingStage
    {
    public:
        /// @param input_resistance_ohms R1, the fixed input resistor.
        /// @param feedback_resistance_ohms Rf, the drive-controlled feedback resistance.
        /// @param saturation_current_amps Diode saturation current Is (Shockley model).
        /// @param thermal_voltage_volts Diode thermal voltage Vt (~0.02585 V at room temperature).
        TubeScreamerClippingStage(
            float input_resistance_ohms, float feedback_resistance_ohms,
            float saturation_current_amps, float thermal_voltage_volts);

        /// @brief Processes one sample through the inverting clipper.
        /// @param input_voltage The stage's input voltage for this sample.
        /// @return The clipped, inverted output voltage. Always finite.
        float process_sample(float input_voltage);

        /// @brief Changes the feedback resistance (the drive control) for subsequent samples.
        /// @param feedback_resistance_ohms The new Rf value.
        void set_feedback_resistance(float feedback_resistance_ohms);

        /// @brief Clears the Newton-Raphson warm-start state.
        void reset();

    private:
        float input_resistance_;
        float feedback_resistance_;
        float saturation_current_;
        float thermal_voltage_;
        float voltage_limit_;
        float previous_voltage_ = 0.0f;
    };
}
