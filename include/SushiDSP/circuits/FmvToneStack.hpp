/**************************************************************************/
/* FmvToneStack.hpp                                                       */
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
 * @file FmvToneStack.hpp
 * @brief The passive Fender/Marshall/Vox tone stack as one third-order section.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

namespace SushiDSP
{
    /// @brief Component values of a Fender/Marshall/Vox passive tone stack.
    struct FmvComponents
    {
        float treble_capacitor_farads;   ///< C1, the series cap into the treble pot.
        float bass_capacitor_farads;     ///< C2, into the bass pot.
        float middle_capacitor_farads;   ///< C3, into the middle pot.
        float slope_resistor_ohms;       ///< R1, the slope resistor from input to the bass/mid node.
        float bass_potentiometer_ohms;   ///< R2, bass pot as a rheostat.
        float middle_potentiometer_ohms; ///< R3, middle pot end to end, tapped by C3 at the wiper.
        float treble_potentiometer_ohms; ///< R4, treble pot end to end.
    };

    /// @brief The JCM800 2203's values: 470 pF, 22 nF, 22 nF, 33 kOhm, 1 MOhm, 25 kOhm, 220 kOhm.
    inline constexpr FmvComponents kJcm2203ToneStack{ 470e-12f, 22e-9f, 22e-9f, 33e3f, 1e6f, 25e3f, 220e3f };

    /// @brief The passive FMV stack as one third-order section, coefficients from Yeh & Smith
    /// (DAFx 2006) discretised with the bilinear transform, recomputed on every knob move.
    class FmvToneStack
    {
    public:
        /// @param sample_rate The rate the section runs at, in hertz.
        /// @param components The stack's component values.
        FmvToneStack(double sample_rate, const FmvComponents& components);

        /// @param normalized Bass pot position in [0, 1]. Values outside are clamped.
        void set_bass(float normalized);

        /// @param normalized Middle pot position in [0, 1]. Values outside are clamped.
        void set_middle(float normalized);

        /// @param normalized Treble pot position in [0, 1]. Values outside are clamped.
        void set_treble(float normalized);

        /// @brief Processes one sample. Audio-thread safe.
        /// @param input The input sample.
        /// @return The tone-shaped output sample.
        float process_sample(float input);

        /// @brief Clears the delay line.
        void reset();

    private:
        /// @brief Rebuilds the discrete coefficients from the current knob positions.
        void recompute();

        double sample_rate_;
        FmvComponents components_;
        float bass_ = 0.5f, middle_ = 0.5f, treble_ = 0.5f;
        double b_[4]{}, a_[4]{};   ///< Discrete numerator and denominator, a_[0] normalised to 1.
        double state_x_[3]{}, state_y_[3]{};
    };
}
