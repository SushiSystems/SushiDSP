/****************************************************************/
/* MesaMark2cToneStack.cpp                                      */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file MesaMark2cToneStack.cpp
 * @brief The Mark IIC+ tone stack's component values and the calls that reach FmvToneStack.
 * @author Mustafa Garip & Sushi Systems
 */

#include "SushiDSP/mesa_mark2c/MesaMark2cToneStack.hpp"

namespace SushiDSP
{
    namespace
    {
        // The seven values are dossier section 4.1's; see modules/nodes/mesa_mark2c/README.md.
        constexpr FmvComponents kMesaMark2cComponents{
            250e-12f, 100e-9f, 47e-9f, 100e3f, 250e3f, 10e3f, 250e3f
        };

        /// @brief V1A's plate impedance, which the deck sits directly on: 150 k in parallel with
        /// the triode's 62.5 k plate resistance. That r_a is the one behind kV1aGain's
        /// 100 x 150 / 212.5 in MesaMark2c.cpp, so both numbers move together.
        constexpr float kV1aPlateImpedanceOhms = 150e3f * 62.5e3f / 212.5e3f;

        /// @brief Volume 1, the 1 M audio pot the deck's output feeds; dossier section 2.
        constexpr float kVolume1LoadOhms = 1e6f;

        /// @brief What drives the deck and what hangs on it. Until 2026-09-20 the model gave it
        /// neither, which cost 4.1 dB of level at 1 kHz ahead of four gain stages.
        constexpr FmvSourceLoad kMesaMark2cTermination{ kV1aPlateImpedanceOhms, kVolume1LoadOhms };

        /// @brief The second treble capacitor LDR1 switches in beside the first; dossier 4.1.
        constexpr float kBrightTrebleCapacitorFarads = 250e-12f;
    }

    MesaMark2cToneStack::MesaMark2cToneStack(double sample_rate)
        : sample_rate_(sample_rate), stack_(sample_rate, kMesaMark2cComponents,
                                            kMesaMark2cTermination)
    {
    }

    void MesaMark2cToneStack::build()
    {
        FmvComponents components = kMesaMark2cComponents;
        if (bright_)
        {
            components.treble_capacitor_farads += kBrightTrebleCapacitorFarads;
        }
        stack_ = FmvToneStack(sample_rate_, components, kMesaMark2cTermination);
        stack_.set_bass(bass_);
        stack_.set_middle(middle_);
        stack_.set_treble(treble_);
    }

    void MesaMark2cToneStack::set_bright(bool engaged)
    {
        if (engaged == bright_)
        {
            return;
        }
        bright_ = engaged;
        build();
    }

    void MesaMark2cToneStack::set_bass(float normalized)
    {
        bass_ = normalized;
        stack_.set_bass(normalized);
    }

    void MesaMark2cToneStack::set_middle(float normalized)
    {
        middle_ = normalized;
        stack_.set_middle(normalized);
    }

    void MesaMark2cToneStack::set_treble(float normalized)
    {
        treble_ = normalized;
        stack_.set_treble(normalized);
    }

    float MesaMark2cToneStack::process_sample(float input) { return stack_.process_sample(input); }

    void MesaMark2cToneStack::reset() { stack_.reset(); }
}
