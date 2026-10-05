/****************************************************************/
/* JcmToneStack.hpp                                             */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file JcmToneStack.hpp
 * @brief Declares the Marshall bass, middle and treble tone stack.
 * @author Mustafa Garip
 */

#pragma once

#include "SushiDSP/math/Biquad.hpp"

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
}
