/**************************************************************************/
/* MesaMark2cToneStack.cpp                                                */
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
        // Dossier section 4.1: 250 pF treble, 0.1 uF bass, 0.047 uF mid, 100 k slope, 250 k bass,
        // 10 k mid, 250 k treble.
        constexpr FmvComponents kMesaMark2cComponents{
            250e-12f, 100e-9f, 47e-9f, 100e3f, 250e3f, 10e3f, 250e3f
        };
    }

    MesaMark2cToneStack::MesaMark2cToneStack(double sample_rate)
        : stack_(sample_rate, kMesaMark2cComponents)
    {
    }

    void MesaMark2cToneStack::set_bass(float normalized) { stack_.set_bass(normalized); }
    void MesaMark2cToneStack::set_middle(float normalized) { stack_.set_middle(normalized); }
    void MesaMark2cToneStack::set_treble(float normalized) { stack_.set_treble(normalized); }

    float MesaMark2cToneStack::process_sample(float input) { return stack_.process_sample(input); }

    void MesaMark2cToneStack::reset() { stack_.reset(); }
}
