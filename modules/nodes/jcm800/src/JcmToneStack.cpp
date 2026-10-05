/****************************************************************/
/* JcmToneStack.cpp                                             */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

#include "SushiDSP/jcm800/JcmToneStack.hpp"

#include <algorithm>

namespace SushiDSP
{
    namespace
    {
        constexpr double kBassHz = 120.0;
        constexpr double kMidHz = 750.0;
        constexpr double kTrebleHz = 2500.0;
        constexpr double kMaxGainDb = 12.0;

        double normalized_to_gain_db(float normalized)
        {
            return (static_cast<double>(normalized) - 0.5) * 2.0 * kMaxGainDb;
        }
    }

    JcmToneStack::JcmToneStack(double sample_rate)
        : sample_rate_(sample_rate)
    {
        set_bass(0.5f);
        set_middle(0.5f);
        set_treble(0.5f);
    }

    void JcmToneStack::set_bass(float normalized)
    {
        normalized = std::clamp(normalized, 0.0f, 1.0f);
        bass_.set_low_shelf(sample_rate_, kBassHz, normalized_to_gain_db(normalized), 0.707);
    }

    void JcmToneStack::set_middle(float normalized)
    {
        normalized = std::clamp(normalized, 0.0f, 1.0f);
        middle_.set_peaking(sample_rate_, kMidHz, normalized_to_gain_db(normalized), 0.9);
    }

    void JcmToneStack::set_treble(float normalized)
    {
        normalized = std::clamp(normalized, 0.0f, 1.0f);
        treble_.set_high_shelf(sample_rate_, kTrebleHz, normalized_to_gain_db(normalized), 0.707);
    }

    float JcmToneStack::process_sample(float input)
    {
        float x = bass_.process_sample(input);
        x = middle_.process_sample(x);
        x = treble_.process_sample(x);
        return x;
    }

    void JcmToneStack::reset()
    {
        bass_.reset();
        middle_.reset();
        treble_.reset();
    }
}
