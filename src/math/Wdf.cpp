/**************************************************************************/
/* Wdf.cpp                                                                */
/**************************************************************************/
/*                          This file is part of:                         */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                         https://sushisystems.io                        */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the PolyForm Noncommercial License 1.0.0 (the           */
/* "License"); you may not use this file except in compliance with the    */
/* License. You may obtain a copy of the License at                       */
/*                                                                        */
/*     https://polyformproject.org/licenses/noncommercial/1.0.0           */
/*                                                                        */
/* Noncommercial use is free. Commercial use requires a separate licence  */
/* from Sushi Systems; see COMMERCIAL.md. The software is provided        */
/* "as is", without warranty of any kind.                                 */
/**************************************************************************/

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
