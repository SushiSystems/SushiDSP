/****************************************************************/
/* SiliconDiode.hpp                                             */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file SiliconDiode.hpp
 * @brief Defines the Shockley-law silicon diode model for a DK-method solver.
 * @author Mustafa Garip
 */

#pragma once

#include <cmath>
#include <cstddef>

namespace SushiDSP
{
    /// @brief Models a Shockley-law silicon diode with an ideality factor as a nonlinear branch of
    /// a DK-method solver; the defaults match docs/reference/spice/models/d1n914.sub.
    ///
    /// The law is exact for |v| <= 40*n*Vt and continues on its own tangent beyond either knee,
    /// so the current is C^1 and finite everywhere (include/SushiDSP/README.md).
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
              scaled_thermal_voltage_(ideality_factor * thermal_voltage_volts),
              inverse_scaled_thermal_voltage_(1.0 / scaled_thermal_voltage_),
              knee_voltage_(kMaxExponentArgument * scaled_thermal_voltage_),
              knee_current_(saturation_current_ * (std::exp(kMaxExponentArgument) - 1.0)),
              knee_jacobian_(saturation_current_ * inverse_scaled_thermal_voltage_ * std::exp(kMaxExponentArgument)),
              reverse_knee_current_(saturation_current_ * (std::exp(-kMaxExponentArgument) - 1.0)),
              reverse_knee_jacobian_(saturation_current_ * inverse_scaled_thermal_voltage_
                                     * std::exp(-kMaxExponentArgument)),
              positive_knee_antiderivative_(saturation_current_
                                            * (scaled_thermal_voltage_ * std::exp(kMaxExponentArgument)
                                               - knee_voltage_)),
              negative_knee_antiderivative_(saturation_current_
                                            * (scaled_thermal_voltage_ * std::exp(-kMaxExponentArgument)
                                               + knee_voltage_))
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
                current_amps = reverse_knee_current_
                               + reverse_knee_jacobian_ * (voltage + knee_voltage_);
                conductance_siemens = reverse_knee_jacobian_;
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

        /// @brief Evaluates the diode current and its antiderivative from a single exponential.
        ///
        /// The antiderivative is C^1 wherever the current is C^0, and only a difference of two
        /// evaluations means anything (include/SushiDSP/README.md).
        /// @param voltage The voltage across the diode (anode minus cathode), in volts.
        /// @param current_amps Receives the diode current in amperes. Always finite.
        /// @param antiderivative_ampere_volts Receives the integral of the current. Always finite.
        void current_and_antiderivative(double voltage, double& current_amps,
                                        double& antiderivative_ampere_volts) const
        {
            if (voltage > knee_voltage_)
            {
                const double excess = voltage - knee_voltage_;
                current_amps = knee_current_ + knee_jacobian_ * excess;
                antiderivative_ampere_volts
                    = positive_knee_antiderivative_
                      + excess * (knee_current_ + 0.5 * knee_jacobian_ * excess);
                return;
            }
            if (voltage < -knee_voltage_)
            {
                const double excess = voltage + knee_voltage_;
                current_amps = reverse_knee_current_ + reverse_knee_jacobian_ * excess;
                antiderivative_ampere_volts
                    = negative_knee_antiderivative_
                      + excess * (reverse_knee_current_ + 0.5 * reverse_knee_jacobian_ * excess);
                return;
            }

            const double exponential = std::exp(voltage * inverse_scaled_thermal_voltage_);
            current_amps = saturation_current_ * (exponential - 1.0);
            antiderivative_ampere_volts
                = saturation_current_ * (scaled_thermal_voltage_ * exponential - voltage);
        }

        /// @brief Evaluates the antiderivative of the diode current with respect to its voltage.
        /// @param voltage The voltage across the diode (anode minus cathode), in volts.
        /// @return The integral of the diode current in ampere-volts. Always finite.
        double current_antiderivative(double voltage) const
        {
            double current_amps = 0.0;
            double antiderivative_ampere_volts = 0.0;
            current_and_antiderivative(voltage, current_amps, antiderivative_ampere_volts);
            return antiderivative_ampere_volts;
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
        double scaled_thermal_voltage_;
        double inverse_scaled_thermal_voltage_;
        double knee_voltage_;
        double knee_current_;
        double knee_jacobian_;
        double reverse_knee_current_;
        double reverse_knee_jacobian_;
        double positive_knee_antiderivative_;
        double negative_knee_antiderivative_;
    };
}
