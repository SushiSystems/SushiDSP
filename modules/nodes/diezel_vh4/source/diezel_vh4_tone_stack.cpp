/**************************************************************************/
/* diezel_vh4_tone_stack.cpp                                              */
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
 * @file diezel_vh4_tone_stack.cpp
 * @brief Builds channel 3's FMV deck from the dossier's five component values.
 * @author Mustafa Garip & Sushi Systems
 */

#include <SushiDSP/nodes/diezel_vh4/diezel_vh4_tone_stack.hpp>

namespace SushiDSP
{
    namespace
    {
        // Dossier section 4 "Values" gives the seven components; R40 is not one of them.
        constexpr FmvComponents K_DIEZEL_VH4_CHANNEL3_STACK{560e-12f, 22e-9f, 22e-9f, 39e3f,
                                                            1e6f,     22e3f,  220e3f};
    } // namespace

    DiezelVh4ToneStack::DiezelVh4ToneStack(double sample_rate)
        : stack_(sample_rate, K_DIEZEL_VH4_CHANNEL3_STACK)
    {
    }

    void DiezelVh4ToneStack::set_bass(float normalized)
    {
        stack_.set_bass(normalized);
    }
    void DiezelVh4ToneStack::set_middle(float normalized)
    {
        stack_.set_middle(normalized);
    }
    void DiezelVh4ToneStack::set_treble(float normalized)
    {
        stack_.set_treble(normalized);
    }

    float DiezelVh4ToneStack::process_sample(float input)
    {
        return stack_.process_sample(input * K_INPUT_RESISTOR_ATTENUATION);
    }

    void DiezelVh4ToneStack::reset()
    {
        stack_.reset();
    }
} // namespace SushiDSP
