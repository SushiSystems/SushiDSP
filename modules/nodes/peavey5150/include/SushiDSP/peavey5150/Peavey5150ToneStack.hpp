/****************************************************************/
/* Peavey5150ToneStack.hpp                                      */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file Peavey5150ToneStack.hpp
 * @brief Declares the 5150's passive LOW, MID and HIGH deck.
 * @author Mustafa Garip
 */

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
        /// The input network carries R24's loss, so the caller hands over V5A's plate voltage
        /// unattenuated.
        /// @param input The sample at V5A's plate.
        /// @return The sample at the HIGH wiper, feeding the post-gain pots.
        float process_sample(float input);

        /// @brief Clears the input network's and the stack's state.
        void reset();

    private:
        Biquad coupling_high_pass_;   ///< C58 into V5A's source resistance and R24.
        Biquad input_shunt_shelf_;    ///< R24's loss and the C59/R94 leg's treble cut on it.
        FmvToneStack stack_;
    };
}
