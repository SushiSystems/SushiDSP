/****************************************************************/
/* DiezelVh4ToneStack.cpp                                       */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file DiezelVh4ToneStack.cpp
 * @brief Builds channel 3's FMV deck from the dossier's five component values.
 * @author Mustafa Garip & Sushi Systems
 */

#include "SushiDSP/diezel_vh4/DiezelVh4ToneStack.hpp"

namespace SushiDSP
{
    namespace
    {
        // Dossier section 4 "Values" gives the seven components; R40 is not one of them.
        constexpr FmvComponents kDiezelVh4Channel3Stack{ 560e-12f, 22e-9f, 22e-9f, 39e3f, 1e6f, 22e3f, 220e3f };
    }

    DiezelVh4ToneStack::DiezelVh4ToneStack(double sample_rate)
        : stack_(sample_rate, kDiezelVh4Channel3Stack)
    {
    }

    void DiezelVh4ToneStack::set_bass(float normalized) { stack_.set_bass(normalized); }
    void DiezelVh4ToneStack::set_middle(float normalized) { stack_.set_middle(normalized); }
    void DiezelVh4ToneStack::set_treble(float normalized) { stack_.set_treble(normalized); }

    float DiezelVh4ToneStack::process_sample(float input)
    {
        return stack_.process_sample(input * kInputResistorAttenuation);
    }

    void DiezelVh4ToneStack::reset() { stack_.reset(); }
}
