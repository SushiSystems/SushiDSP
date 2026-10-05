/****************************************************************/
/* Wdf.cpp                                                      */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file Wdf.cpp
 * @brief Implements the wave digital filter resistor and capacitor one-ports.
 * @author Mustafa Garip
 */

#include "SushiDSP/math/Wdf.hpp"

namespace SushiDSP
{
    WdfResistor::WdfResistor(float resistance_ohms)
        : resistance_(resistance_ohms)
    {
    }

    float WdfResistor::port_resistance() const
    {
        return resistance_;
    }

    float WdfResistor::reflect(float)
    {
        return 0.0f;
    }

    WdfCapacitor::WdfCapacitor(double sample_rate, float capacitance_farads)
        : port_resistance_(static_cast<float>(1.0 / sample_rate) / (2.0f * capacitance_farads))
    {
    }

    float WdfCapacitor::port_resistance() const
    {
        return port_resistance_;
    }

    float WdfCapacitor::reflect(float incident_wave)
    {
        const float reflected = previous_incident_wave_;
        previous_incident_wave_ = incident_wave;
        return reflected;
    }

    void WdfCapacitor::reset()
    {
        previous_incident_wave_ = 0.0f;
    }
}
