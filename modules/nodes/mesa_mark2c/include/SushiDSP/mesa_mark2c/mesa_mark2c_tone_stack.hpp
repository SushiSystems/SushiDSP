/**************************************************************************/
/* mesa_mark2c_tone_stack.hpp                                             */
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
 * @file mesa_mark2c_tone_stack.hpp
 * @brief The Mark IIC+'s FMV tone stack, sitting on V1A's plate.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include "SushiDSP/circuits/fmv_tone_stack.hpp"

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

            /// @brief Switches PULL BRIGHT's second treble capacitor in or out.
            ///
            /// LDR1 puts another 250 pF beside the first, so the deck's series path into the treble
            /// pot opens an octave lower. The whole netlist is rebuilt, because the capacitance is a
            /// component value rather than a pot position; the three pot positions are carried over.
            /// @param engaged True puts the second capacitor in.
            void set_bright(bool engaged);

            /// @brief Clears the stack's delay line.
            void reset();

        private:
            /// @brief Rebuilds the deck from the components the bright switch selects.
            void build();

            double sample_rate_;
            bool bright_ = false;
            float bass_ = 0.5f, middle_ = 0.5f, treble_ = 0.5f;
            FmvToneStack stack_;
    };
} // namespace SushiDSP
