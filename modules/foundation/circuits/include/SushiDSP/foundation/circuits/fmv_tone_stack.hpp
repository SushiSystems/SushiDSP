/**************************************************************************/
/* fmv_tone_stack.hpp                                                     */
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
 * @file fmv_tone_stack.hpp
 * @brief The passive Fender/Marshall/Vox tone stack as a DK netlist, middle pot a rheostat.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <SushiDSP/foundation/math/dk/dk_circuit.hpp>
#include <SushiDSP/foundation/math/dk/dk_netlist.hpp>
#include <SushiDSP/foundation/math/dk/linear_port.hpp>

namespace SushiDSP
{
    /// @brief Component values of a Fender/Marshall/Vox passive tone stack.
    struct FmvComponents
    {
            float treble_capacitor_farads; ///< C1, the series cap into the treble pot.
            float bass_capacitor_farads;   ///< C2, into the bass pot.
            float middle_capacitor_farads; ///< C3, into the middle pot.
            float slope_resistor_ohms; ///< The slope resistor from the input to the bass/mid node.
            float bass_potentiometer_ohms;   ///< R2, bass pot as a rheostat.
            float middle_potentiometer_ohms; ///< R3, middle pot as a rheostat under C3, to ground.
            float treble_potentiometer_ohms; ///< R1, the treble pot end to end.
    };

    /// @brief What the stack is driven by and what hangs on its output.
    ///
    /// The stack is a passive divider, so both terminations change its response. A stage's plate
    /// impedance ahead of it and the next stage's grid network across it are ordinary resistors in
    /// the netlist; zero means the element is absent, which is an ideal source and an open output.
    struct FmvSourceLoad
    {
            float source_resistance_ohms =
                0.0f; ///< Impedance driving the input; 0 is an ideal source.
            float load_resistance_ohms = 0.0f; ///< Resistance across the output; 0 leaves it open.
    };

    /// @brief The JCM800 2203's values: 470 pF, 22 nF, 22 nF, 33 kOhm, 1 MOhm, 25 kOhm, 220 kOhm.
    inline constexpr FmvComponents K_JCM2203_TONE_STACK{470e-12f, 22e-9f, 22e-9f, 33e3f,
                                                        1e6f,     25e3f,  220e3f};

    /// @brief The passive FMV stack, solved from its netlist by the nodal DK method.
    ///
    /// The deck is the Bassman arrangement: the treble cap feeds the treble pot, whose wiper is
    /// the output and whose bottom end joins the bass pot; the slope resistor feeds a node that
    /// reaches the bass pot through the bass cap and the middle pot through the middle cap; the
    /// middle pot is a rheostat to ground under the bass pot. That netlist reproduces the
    /// hand-derived closed form this class used to carry to 1.2e-14 across 480 knob and frequency
    /// combinations, which is what `test_fmv_tone_stack.cpp` asserts.
    class FmvToneStack
    {
        public:
            /// @param sample_rate The rate the section runs at, in hertz.
            /// @param components The stack's component values.
            /// @param termination What drives the input and what loads the output.
            FmvToneStack(double sample_rate, const FmvComponents& components,
                         const FmvSourceLoad& termination = {});

            /// @brief Sets the bass pot position and re-derives the matrices.
            /// @param normalized Bass pot position in [0, 1]. Values outside are clamped.
            void set_bass(float normalized);

            /// @brief Sets the middle pot position and re-derives the matrices.
            /// @param normalized Middle pot position in [0, 1]. Values outside are clamped.
            void set_middle(float normalized);

            /// @brief Sets the treble pot position and re-derives the matrices.
            /// @param normalized Treble pot position in [0, 1]. Values outside are clamped.
            void set_treble(float normalized);

            /// @brief Processes one sample. Audio-thread safe.
            /// @param input The input sample.
            /// @return The tone-shaped output sample.
            float process_sample(float input);

            /// @brief Clears the reactive state.
            void reset();

        private:
            /// @brief Number of capacitors, and so of DK states.
            static constexpr std::size_t K_STATE_COUNT = 3;

            /// @brief Builds the netlist from the components and the termination.
            void build();

            double sample_rate_;
            FmvComponents components_;
            FmvSourceLoad termination_;
            float bass_ = 0.5f, middle_ = 0.5f, treble_ = 0.5f;
            std::size_t treble_pot_ = 0; ///< DK index of the treble pot.
            std::size_t bass_pot_ = 0;   ///< DK index of the bass rheostat.
            std::size_t middle_pot_ = 0; ///< DK index of the middle rheostat.
            dk::LinearPort port_{};
            dk::DkCircuit<K_STATE_COUNT, 0, 1, 1> circuit_{};
    };
} // namespace SushiDSP
