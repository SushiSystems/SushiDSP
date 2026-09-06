/**************************************************************************/
/* MesaMark2cToneStack.hpp                                                */
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
 * @file MesaMark2cToneStack.hpp
 * @brief The Mark IIC+'s FMV tone stack, sitting on V1A's plate.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include "SushiDSP/circuits/FmvToneStack.hpp"

namespace SushiDSP
{
    /// @brief The Mark IIC+ passive tone stack of dossier section 4.1, wrapping one FmvToneStack.
    /// It hangs directly on V1A's plate, ahead of every lead gain stage, which is what makes it
    /// shape what the cascade distorts rather than the finished distortion (dossier section 2).
    class MesaMark2cToneStack
    {
    public:
        /// @param sample_rate The rate the stack runs at, in hertz.
        explicit MesaMark2cToneStack(double sample_rate);

        /// @brief Sets the bass pot position.
        /// @param normalized Bass pot position in [0, 1]; values outside are clamped.
        void set_bass(float normalized);

        /// @brief Sets the middle pot position.
        /// @param normalized Middle pot position in [0, 1]; values outside are clamped.
        void set_middle(float normalized);

        /// @brief Sets the treble pot position.
        /// @param normalized Treble pot position in [0, 1]; values outside are clamped.
        void set_treble(float normalized);

        /// @brief Processes one sample through the stack. Audio-thread safe.
        /// @param input The input sample.
        /// @return The tone-shaped output sample.
        float process_sample(float input);

        /// @brief Clears the stack's delay line.
        void reset();

    private:
        FmvToneStack stack_;
    };
}
