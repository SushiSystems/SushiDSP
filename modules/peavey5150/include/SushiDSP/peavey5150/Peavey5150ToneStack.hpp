/**************************************************************************/
/* Peavey5150ToneStack.hpp                                                */
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

#pragma once

#include "SushiDSP/circuits/FmvToneStack.hpp"
#include "SushiDSP/math/Biquad.hpp"

namespace SushiDSP
{
    /// @brief The 5150's shared passive LOW/MID/HIGH deck: the FMV stack of dossier section 4
    /// plus the two shunt legs on its input node that the closed form does not contain. 0.5 on
    /// any control is the pot at noon, not a flat response - the deck is passive and lossy.
    class Peavey5150ToneStack
    {
    public:
        /// @param sample_rate The rate the deck runs at, in hertz.
        explicit Peavey5150ToneStack(double sample_rate);

        /// @brief Sets the LOW pot position.
        /// @param normalized VR3's position in [0, 1]; values outside are clamped.
        void set_low(float normalized);

        /// @brief Sets the MID pot position.
        /// @param normalized VR4's position in [0, 1]; values outside are clamped.
        void set_mid(float normalized);

        /// @brief Sets the HIGH pot position.
        /// @param normalized VR5's position in [0, 1]; values outside are clamped.
        void set_high(float normalized);

        /// @brief Processes one sample through the input network and the stack. Audio-thread safe.
        /// @param input The sample at V5A's plate.
        /// @return The sample at the HIGH wiper, feeding the post-gain pots.
        float process_sample(float input);

        /// @brief Clears the input network's and the stack's state.
        void reset();

        /// @brief The frequency-independent loss R24 33 kOhm imposes against V5A's plate.
        static constexpr float kInputShuntAttenuation = 0.4824525f;

    private:
        Biquad coupling_high_pass_;   ///< C58 into V5A's source resistance and R24.
        Biquad input_shunt_shelf_;    ///< The C59/R94 leg's treble cut on the same node.
        FmvToneStack stack_;
    };
}
