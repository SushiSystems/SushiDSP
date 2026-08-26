/**************************************************************************/
/* DiodeClipperCircuit.hpp                                                */
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
    /// @brief A minimal one-stage nonlinear circuit: Vs -> Rs -> antiparallel diode pair ->
    /// ground.
    ///
    /// Despite what this comment used to claim, there is no wave-digital filter here. The class
    /// is a scalar Newton-Raphson solve of the nodal equation for a series resistor driving an
    /// antiparallel diode pair:
    /// @code
    /// v = Vs - Rs * Is * (exp(v / Vt) - exp(-v / Vt))
    /// @endcode
    /// solved for the diode-pair voltage v each sample, warm-started from the previous sample.
    /// Not a product — no NodeDescriptor; JCM800/Tube Screamer (D3/D4) reuse this pattern, not
    /// this exact class.
    ///
    /// D6b re-expresses the same circuit as a `DkCircuit<0,2,1,1>`, carrying the pair as two
    /// antiparallel diode branches; the two implementations are independent and
    /// `test_dk_circuit.cpp` asserts they agree to 1e-6. Kept as a second oracle; removal is a
    /// D6c decision once `TubeScreamer` no longer references it.
    class DiodeClipperCircuit
    {
    public:
        /// @param source_resistance_ohms Rs, the source's series resistance.
        /// @param saturation_current_amps Diode saturation current Is (Shockley model).
        /// @param thermal_voltage_volts Diode thermal voltage Vt (~0.02585 V at room temperature).
        DiodeClipperCircuit(float source_resistance_ohms, float saturation_current_amps, float thermal_voltage_volts);

        /// @brief Processes one sample: given the source voltage, returns the clipped output voltage.
        /// @param source_voltage Vs for this sample.
        /// @return The voltage across the diode pair (the clipped output). Always finite.
        float process_sample(float source_voltage);

        /// @brief Clears the Newton-Raphson warm-start state.
        void reset();

    private:
        float source_resistance_;
        float saturation_current_;
        float thermal_voltage_;
        float voltage_limit_;
        float previous_voltage_ = 0.0f;
    };
}
