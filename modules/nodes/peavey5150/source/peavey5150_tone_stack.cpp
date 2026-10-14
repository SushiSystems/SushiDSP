/**************************************************************************/
/* peavey5150_tone_stack.cpp                                              */
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
 * @file peavey5150_tone_stack.cpp
 * @brief Implements the 5150's passive tone deck and the input network ahead of it.
 * @author Mustafa Garip
 */

#include <SushiDSP/nodes/peavey5150/peavey5150_tone_stack.hpp>

namespace SushiDSP
{
    namespace
    {
        // The deck's values are dossier section 4's; see modules/nodes/peavey5150/README.md.
        constexpr FmvComponents K_PEAVEY5150_STACK{570e-12f, 22e-9f, 22e-9f, 47e3f,
                                                   1e6f,     50e3f,  250e3f};

        // Dossier section 3, V5A row: R86 100 kOhm || the solved 54.8 kOhm ra drives the input.
        constexpr double K_V5A_SOURCE_RESISTANCE_OHMS = 35400.5;

        // Dossier section 4: R24 33 kOhm to ground loses 33k/(35.4k + 33k) = 0.4824525, once.
        constexpr double K_INPUT_SHUNT_OHMS = 33000.0;
        constexpr double K_INPUT_SHUNT_ATTENUATION = 0.4824525;

        // Dossier section 4: C59 0.01 uF in series with R94 47 kOhm, on the same node as R24.
        constexpr double K_INPUT_SHELF_CAPACITOR_FARADS = 10e-9;
        constexpr double K_INPUT_SHELF_RESISTANCE_OHMS = 47000.0;

        // Dossier section 2: C58 0.022 uF from V5A's plate into the stack.
        constexpr double K_COUPLING_CAPACITOR_FARADS = 22e-9;

        // The capacitor works into V5A's source plus R24: 1/(2*pi*22n*(35.4k + 33k)) = 105.76 Hz.
        double coupling_corner_hz()
        {
            return 1.0 / (2.0 * 3.14159265358979323846 * K_COUPLING_CAPACITOR_FARADS *
                          (K_V5A_SOURCE_RESISTANCE_OHMS + K_INPUT_SHUNT_OHMS));
        }

        // The C59/R94 leg makes a 2.71 dB treble cut: zero 338.63 Hz, pole 248.37 Hz.
        double input_shelf_zero_hz()
        {
            return 1.0 / (2.0 * 3.14159265358979323846 * K_INPUT_SHELF_CAPACITOR_FARADS *
                          K_INPUT_SHELF_RESISTANCE_OHMS);
        }

        double input_shelf_pole_hz()
        {
            const double source = K_V5A_SOURCE_RESISTANCE_OHMS;
            const double shunt = K_INPUT_SHUNT_OHMS;
            const double leg = K_INPUT_SHELF_RESISTANCE_OHMS;
            return (source + shunt) /
                   (2.0 * 3.14159265358979323846 * K_INPUT_SHELF_CAPACITOR_FARADS *
                    (source * shunt + source * leg + shunt * leg));
        }
    } // namespace

    Peavey5150ToneStack::Peavey5150ToneStack(double sample_rate)
        : stack_(sample_rate, K_PEAVEY5150_STACK)
    {
        coupling_high_pass_.set_first_order_high_pass(sample_rate, coupling_corner_hz());
        input_shunt_shelf_.set_first_order_shelf(sample_rate, input_shelf_zero_hz(),
                                                 input_shelf_pole_hz(), K_INPUT_SHUNT_ATTENUATION);
        set_low(0.5f);
        set_mid(0.5f);
        set_high(0.5f);
    }

    void Peavey5150ToneStack::set_low(float normalized)
    {
        stack_.set_bass(normalized);
    }
    void Peavey5150ToneStack::set_mid(float normalized)
    {
        stack_.set_middle(normalized);
    }
    void Peavey5150ToneStack::set_high(float normalized)
    {
        stack_.set_treble(normalized);
    }

    float Peavey5150ToneStack::process_sample(float input)
    {
        const float coupled = coupling_high_pass_.process_sample(input);
        return stack_.process_sample(input_shunt_shelf_.process_sample(coupled));
    }

    void Peavey5150ToneStack::reset()
    {
        coupling_high_pass_.reset();
        input_shunt_shelf_.reset();
        stack_.reset();
    }
} // namespace SushiDSP
