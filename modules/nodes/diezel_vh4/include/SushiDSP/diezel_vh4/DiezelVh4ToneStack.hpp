/****************************************************************/
/* DiezelVh4ToneStack.hpp                                       */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file DiezelVh4ToneStack.hpp
 * @brief The Diezel VH4 channel-3 Bass/Middle/Treble network as an FMV stack.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include "SushiDSP/circuits/FmvToneStack.hpp"

namespace SushiDSP
{
    /// @brief Channel 3's R40/R41/C27/C28/C29 deck, driven from the V3b cathode follower.
    ///
    /// The five component values are read off the dossier (section 4); their interconnection is
    /// not.
    /// The drawing lands the bass and mid capacitors on their pots' wiper terminals, which shorts
    /// the treble pot's upper section, and the dossier's blocking question 1 records that the
    /// wiring is a reconstruction. This class builds the standard FMV interconnection the dossier
    /// recommends, and folds R40 in as a fixed attenuation - see modules/nodes/diezel_vh4/README.md.
    class DiezelVh4ToneStack
    {
    public:
        /// @param sample_rate The rate the stack runs at, in hertz.
        explicit DiezelVh4ToneStack(double sample_rate);

        /// @brief Sets the bass pot position.
        /// @param normalized Bass pot position in [0, 1]; values outside are clamped.
        void set_bass(float normalized);

        /// @brief Sets the middle pot position.
        /// @param normalized Middle pot position in [0, 1]; values outside are clamped.
        void set_middle(float normalized);

        /// @brief Sets the treble pot position.
        /// @param normalized Treble pot position in [0, 1]; values outside are clamped.
        void set_treble(float normalized);

        /// @brief Processes one sample through R40 and the stack. Audio-thread safe.
        /// @param input The sample arriving from the V3b cathode follower.
        /// @return The tone-shaped sample leaving the treble wiper into R42.
        float process_sample(float input);

        /// @brief Clears the stack's delay line.
        void reset();

        /// @brief The loss R40 contributes ahead of the stack.
        /// @return The frequency-independent factor process_sample() applies before the stack.
        static constexpr float input_resistor_attenuation() { return kInputResistorAttenuation; }

    private:
        /// @brief R40 10 kOhm against the stack's own resistive path to ground, as one number.
        ///
        /// The dossier states no input impedance for the deck, so the divider is taken against
        /// the path the stack presents at midband: R41 39 kOhm plus the bass pot's 1 MOhm plus
        /// the mid pot's 22 kOhm, 1.061 MOhm, giving 1.061e6 / (1.061e6 + 1e4) = 0.990664, or
        /// -0.0815 dB. A documented approximation: the real divider moves with frequency as C27
        /// bypasses R41.
        static constexpr float kInputResistorAttenuation = 0.990664f;

        FmvToneStack stack_;
    };
}
