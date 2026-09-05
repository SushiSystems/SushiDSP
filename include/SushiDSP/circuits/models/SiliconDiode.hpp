/**************************************************************************/
/* SiliconDiode.hpp                                                       */
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

#include <cmath>
#include <cstddef>

namespace SushiDSP
{
    /// @brief Shockley-law silicon diode device model with an ideality factor, for use as a
    /// nonlinear branch in a DK-method circuit solver.
    ///
    /// The default parameters (Is = 2.52 nA, n = 1.752, Vt = 0.025864 V) match the 1N914
    /// small-signal diode as modelled in `docs/reference/spice/models/d1n914.sub`.
    ///
    /// `current()`/`jacobian()` implement:
    /// @code
    /// n*Vt  = ideality_factor * thermal_voltage_volts
    /// i(v)  = Is * (exp(v / (n*Vt)) - 1)
    /// di/dv = (Is / (n*Vt)) * exp(v / (n*Vt))
    /// @endcode
    ///
    /// A Newton-Raphson solver can overshoot far past any physically meaningful diode voltage,
    /// and `exp()` of a large argument overflows to `inf`. To keep the model finite everywhere
    /// while remaining exactly the Shockley law inside the operating range, the exponent argument
    /// is clamped to +-40 (v_max = 40*n*Vt =~ 1.81 V for the 1N914 defaults) — no silicon diode in
    /// a guitar circuit sits above that voltage. Clamping the exponent alone would break C^1
    /// continuity at the knee, so beyond +-v_max the model switches to the tangent line at v_max
    /// instead: `i(v) = i(v_max) + jacobian(v_max) * (v - v_max)` and `jacobian(v) = jacobian(v_max)`,
    /// mirrored for `v < -v_max`. This is C^1 everywhere, finite everywhere, and identical to the
    /// Shockley law for |v| <= v_max.
    ///
    /// A DK solver wants the current and its derivative at the same voltage, so
    /// `current_and_jacobian()` carries the law and `current()`/`jacobian()` forward to it.
    class SiliconDiode
    {
    public:
        /// @brief The number of nonlinear branch equations this device contributes to a DK solve.
        static constexpr std::size_t kBranchCount = 1;

        /// @param saturation_current_amps Is, the diode's reverse saturation current.
        /// @param ideality_factor n, the Shockley ideality (emission coefficient).
        /// @param thermal_voltage_volts Vt, the thermal voltage (~0.025864 V at room temperature).
        explicit SiliconDiode(double saturation_current_amps = 2.52e-9,
                               double ideality_factor = 1.752,
                               double thermal_voltage_volts = 0.025864)
            : saturation_current_(saturation_current_amps),
              inverse_scaled_thermal_voltage_(1.0 / (ideality_factor * thermal_voltage_volts)),
              knee_voltage_(kMaxExponentArgument / inverse_scaled_thermal_voltage_),
              knee_current_(saturation_current_ * (std::exp(kMaxExponentArgument) - 1.0)),
              knee_jacobian_(saturation_current_ * inverse_scaled_thermal_voltage_ * std::exp(kMaxExponentArgument))
        {
        }

        /// @brief Evaluates the diode current and its derivative from a single exponential.
        ///
        /// The DK solver always needs both, and current()/jacobian() each cost one exp(); this
        /// entry point costs one for the pair. current() and jacobian() forward here, so the
        /// Shockley law and its linear extrapolation exist in exactly one place.
        /// @param voltage The voltage across the diode (anode minus cathode), in volts.
        /// @param current_amps Receives the diode current in amperes. Always finite.
        /// @param conductance_siemens Receives di/dv in siemens. Always finite.
        void current_and_jacobian(double voltage, double& current_amps,
                                  double& conductance_siemens) const
        {
            if (voltage > knee_voltage_)
            {
                current_amps = knee_current_ + knee_jacobian_ * (voltage - knee_voltage_);
                conductance_siemens = knee_jacobian_;
                return;
            }
            if (voltage < -knee_voltage_)
            {
                current_amps = -knee_current_ + knee_jacobian_ * (voltage + knee_voltage_);
                conductance_siemens = knee_jacobian_;
                return;
            }

            const double exponential = std::exp(voltage * inverse_scaled_thermal_voltage_);
            current_amps = saturation_current_ * (exponential - 1.0);
            conductance_siemens = saturation_current_ * inverse_scaled_thermal_voltage_ * exponential;
        }

        /// @brief Evaluates the diode current for a given branch voltage.
        /// @param voltage The voltage across the diode (anode minus cathode), in volts.
        /// @return The diode current in amperes. Always finite.
        double current(double voltage) const
        {
            double current_amps = 0.0;
            double conductance_siemens = 0.0;
            current_and_jacobian(voltage, current_amps, conductance_siemens);
            return current_amps;
        }

        /// @brief Evaluates the derivative of the diode current with respect to its voltage.
        /// @param voltage The voltage across the diode (anode minus cathode), in volts.
        /// @return di/dv in siemens. Always finite.
        double jacobian(double voltage) const
        {
            double current_amps = 0.0;
            double conductance_siemens = 0.0;
            current_and_jacobian(voltage, current_amps, conductance_siemens);
            return conductance_siemens;
        }

    private:
        /// @brief The exponent argument beyond which the model switches to a linear extrapolation.
        static constexpr double kMaxExponentArgument = 40.0;

        double saturation_current_;
        double inverse_scaled_thermal_voltage_;
        double knee_voltage_;
        double knee_current_;
        double knee_jacobian_;
    };
}
