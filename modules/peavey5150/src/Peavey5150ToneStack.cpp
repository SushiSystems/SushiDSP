/**************************************************************************/
/* Peavey5150ToneStack.cpp                                                */
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

#include "SushiDSP/peavey5150/Peavey5150ToneStack.hpp"

namespace SushiDSP
{
    namespace
    {
        // Dossier section 4: C16 470 pF in parallel with C60 100 pF, C18 0.022 uF, C21 0.022 uF,
        // R28 47 kOhm slope resistor, VR3 1 MOhm LOW, VR4 50 kOhm MID, VR5 250 kOhm HIGH.
        constexpr FmvComponents kPeavey5150Stack{ 570e-12f, 22e-9f, 22e-9f, 47e3f, 1e6f, 50e3f, 250e3f };

        // Dossier section 3, V5A row: the plate load R86 100 kOhm in parallel with the solved
        // plate resistance 54.8 kOhm, which is the impedance driving the stack's input node.
        constexpr double kV5aSourceResistanceOhms = 35400.5;

        // Dossier section 4: R24 33 kOhm from V5A's coupling node to ground.
        constexpr double kInputShuntOhms = 33000.0;

        // Dossier section 4: C59 0.01 uF in series with R94 47 kOhm, on the same node as R24.
        constexpr double kInputShelfCapacitorFarads = 10e-9;
        constexpr double kInputShelfResistanceOhms = 47000.0;

        // Dossier section 2: C58 0.022 uF from V5A's plate into the stack.
        constexpr double kCouplingCapacitorFarads = 22e-9;

        // Computed from the values above: the coupling capacitor works into V5A's source
        // resistance plus R24, so the corner is 1/(2*pi*22n*(35.4k + 33k)) = 105.76 Hz.
        double coupling_corner_hz()
        {
            return 1.0 / (2.0 * 3.14159265358979323846
                          * kCouplingCapacitorFarads * (kV5aSourceResistanceOhms + kInputShuntOhms));
        }

        // The C59/R94 leg is open at DC and 47 kOhm above its corner, so the input node's shunt
        // falls from 33 kOhm to 33k || 47k = 19.36 kOhm and the divider from 0.4825 to 0.3539:
        // a 2.71 dB treble cut. Zero 338.63 Hz, pole 248.37 Hz.
        double input_shelf_zero_hz()
        {
            return 1.0 / (2.0 * 3.14159265358979323846
                          * kInputShelfCapacitorFarads * kInputShelfResistanceOhms);
        }

        double input_shelf_pole_hz()
        {
            const double source = kV5aSourceResistanceOhms;
            const double shunt = kInputShuntOhms;
            const double leg = kInputShelfResistanceOhms;
            return (source + shunt)
                 / (2.0 * 3.14159265358979323846 * kInputShelfCapacitorFarads
                    * (source * shunt + source * leg + shunt * leg));
        }
    }

    Peavey5150ToneStack::Peavey5150ToneStack(double sample_rate)
        : stack_(sample_rate, kPeavey5150Stack)
    {
        coupling_high_pass_.set_high_pass(sample_rate, coupling_corner_hz(), 0.7071);
        input_shunt_shelf_.set_first_order_shelf(sample_rate, input_shelf_zero_hz(), input_shelf_pole_hz(),
                                                 static_cast<double>(kInputShuntAttenuation));
        set_low(0.5f);
        set_mid(0.5f);
        set_high(0.5f);
    }

    void Peavey5150ToneStack::set_low(float normalized) { stack_.set_bass(normalized); }
    void Peavey5150ToneStack::set_mid(float normalized) { stack_.set_middle(normalized); }
    void Peavey5150ToneStack::set_high(float normalized) { stack_.set_treble(normalized); }

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
}
