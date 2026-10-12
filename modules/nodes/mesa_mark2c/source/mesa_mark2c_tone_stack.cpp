/**************************************************************************/
/* mesa_mark2c_tone_stack.cpp                                             */
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
 * @file mesa_mark2c_tone_stack.cpp
 * @brief The Mark IIC+ tone stack's component values and the calls that reach FmvToneStack.
 * @author Mustafa Garip & Sushi Systems
 */

#include "SushiDSP/mesa_mark2c/mesa_mark2c_tone_stack.hpp"

namespace SushiDSP
{
    namespace
    {
        // The seven values are dossier section 4.1's; see modules/nodes/mesa_mark2c/README.md.
        constexpr FmvComponents K_MESA_MARK2C_COMPONENTS{250e-12f, 100e-9f, 47e-9f, 100e3f,
                                                         250e3f,   10e3f,   250e3f};

        /// @brief V1A's plate impedance, which the deck sits directly on: 150 k in parallel with
        /// the triode's 62.5 k plate resistance. That r_a is the one behind K_V1A_GAIN's
        /// 100 x 150 / 212.5 in mesa_mark2c.cpp, so both numbers move together.
        constexpr float K_V1A_PLATE_IMPEDANCE_OHMS = 150e3f * 62.5e3f / 212.5e3f;

        /// @brief Volume 1, the 1 M audio pot the deck's output feeds; dossier section 2.
        constexpr float K_VOLUME1_LOAD_OHMS = 1e6f;

        /// @brief What drives the deck and what hangs on it. Until 2026-09-20 the model gave it
        /// neither, which cost 4.1 dB of level at 1 kHz ahead of four gain stages.
        constexpr FmvSourceLoad K_MESA_MARK2C_TERMINATION{K_V1A_PLATE_IMPEDANCE_OHMS,
                                                          K_VOLUME1_LOAD_OHMS};

        /// @brief The second treble capacitor LDR1 switches in beside the first; dossier 4.1.
        constexpr float K_BRIGHT_TREBLE_CAPACITOR_FARADS = 250e-12f;
    } // namespace

    MesaMark2cToneStack::MesaMark2cToneStack(double sample_rate)
        : sample_rate_(sample_rate),
          stack_(sample_rate, K_MESA_MARK2C_COMPONENTS, K_MESA_MARK2C_TERMINATION)
    {
    }

    void MesaMark2cToneStack::build()
    {
        FmvComponents components = K_MESA_MARK2C_COMPONENTS;
        if (bright_)
        {
            components.treble_capacitor_farads += K_BRIGHT_TREBLE_CAPACITOR_FARADS;
        }
        stack_ = FmvToneStack(sample_rate_, components, K_MESA_MARK2C_TERMINATION);
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

    float MesaMark2cToneStack::process_sample(float input)
    {
        return stack_.process_sample(input);
    }

    void MesaMark2cToneStack::reset()
    {
        stack_.reset();
    }
} // namespace SushiDSP
