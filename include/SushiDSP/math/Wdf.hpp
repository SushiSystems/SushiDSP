/****************************************************************/
/* Wdf.hpp                                                      */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

#pragma once

namespace SushiDSP
{
    /// @brief A Wave Digital Filter one-port's wave-domain state: incident wave a, reflected
    /// wave b, and the port resistance R that relates them to voltage/current.
    struct WdfPort
    {
        float port_resistance = 1.0f;
        float incident_wave = 0.0f;
        float reflected_wave = 0.0f;
    };

    /// @brief An adapted (reflection-free) linear resistor one-port.
    class WdfResistor
    {
    public:
        /// @param resistance_ohms The resistor's value; also its WDF port resistance.
        explicit WdfResistor(float resistance_ohms);

        /// @brief The port resistance this element presents (equals its resistance).
        /// @return The port resistance in ohms.
        float port_resistance() const;

        /// @brief Computes the reflected wave for a given incident wave.
        /// @param incident_wave The incident wave a.
        /// @return The reflected wave b, always 0 for a matched resistor.
        float reflect(float incident_wave);

    private:
        float resistance_;
    };

    /// @brief A linear capacitor one-port, discretized via the bilinear transform.
    class WdfCapacitor
    {
    public:
        /// @param sample_rate The sample rate in Hz.
        /// @param capacitance_farads The capacitance in farads.
        WdfCapacitor(double sample_rate, float capacitance_farads);

        /// @brief The port resistance R = T / (2C), where T is the sample period.
        /// @return The port resistance in ohms.
        float port_resistance() const;

        /// @brief Computes the reflected wave: b[n] = a[n-1] (the bilinear-transform result).
        /// @param incident_wave The incident wave a[n].
        /// @return The reflected wave b[n] = the previous call's incident wave.
        float reflect(float incident_wave);

        /// @brief Clears the stored previous incident wave.
        void reset();

    private:
        float port_resistance_;
        float previous_incident_wave_ = 0.0f;
    };
}
