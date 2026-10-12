/**************************************************************************/
/* jcm_tone_stack.hpp                                                     */
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
 * @file jcm_tone_stack.hpp
 * @brief Declares the Marshall bass, middle and treble tone stack.
 * @author Mustafa Garip
 */

#pragma once

#include "SushiDSP/math/biquad.hpp"

namespace SushiDSP
{
    /// @brief The Marshall Bass/Mid/Treble tone stack, approximated as three cascaded shelf/peak
    /// filters (a documented simplification of the passive DK-method network — see the D4 plan's
    /// Architecture section). 0.5 on any control is flat/unity.
    class JcmToneStack
    {
        public:
            /// @param sample_rate The sample rate in Hz.
            explicit JcmToneStack(double sample_rate);

            /// @brief Sets the bass (low-shelf) control.
            /// @param normalized Bass amount in [0, 1]; 0.5 is flat.
            void set_bass(float normalized);

            /// @brief Sets the middle (peaking) control.
            /// @param normalized Middle amount in [0, 1]; 0.5 is flat.
            void set_middle(float normalized);

            /// @brief Sets the treble (high-shelf) control.
            /// @param normalized Treble amount in [0, 1]; 0.5 is flat.
            void set_treble(float normalized);

            /// @brief Processes one sample through the cascaded bass/mid/treble filters.
            /// @param input The input sample.
            /// @return The tone-shaped output sample.
            float process_sample(float input);

            /// @brief Clears all three filters' state.
            void reset();

        private:
            double sample_rate_;
            Biquad bass_;
            Biquad middle_;
            Biquad treble_;
    };
} // namespace SushiDSP
