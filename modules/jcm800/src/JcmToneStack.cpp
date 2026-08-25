/**************************************************************************/
/* JcmToneStack.cpp                                                       */
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

#include "SushiDSP/jcm800/JcmToneStack.hpp"

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
        bass_.set_low_shelf(sample_rate_, kBassHz, normalized_to_gain_db(normalized), 0.707);
    }

    void JcmToneStack::set_middle(float normalized)
    {
        middle_.set_peaking(sample_rate_, kMidHz, normalized_to_gain_db(normalized), 0.9);
    }

    void JcmToneStack::set_treble(float normalized)
    {
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
