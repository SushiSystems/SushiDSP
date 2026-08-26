/**************************************************************************/
/* DkNetlist.hpp                                                          */
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

#include <array>
#include <cassert>
#include <cstddef>

namespace SushiDSP::dk
{
    /// @brief Largest number of non-ground nodes any DkCircuit instance may describe.
    inline constexpr std::size_t kMaximumNodes = 16;

    /// @brief Largest number of resistor branches, potentiometer sections included.
    inline constexpr std::size_t kMaximumResistors = 32;

    /// @brief Largest number of potentiometers a single circuit may carry.
    inline constexpr std::size_t kMaximumPotentiometers = 4;

    /// @brief One two-terminal element: its node pair and its value.
    ///
    /// Node 0 is ground. The element's current is defined as flowing from @c positive_node to
    /// @c negative_node through the element, and its voltage as V(positive_node) minus
    /// V(negative_node); the whole derivation reads its incidence signs off that one convention.
    struct Branch
    {
        std::size_t positive_node = 0;  ///< Terminal the branch voltage is measured from; 0 is ground.
        std::size_t negative_node = 0;  ///< Terminal the branch current flows into; 0 is ground.
        double      value = 0.0;        ///< Ohms, farads or henries; unused for sources, nonlinears and probes.
    };

    /// @brief The two resistor sections a potentiometer contributes, plus its track resistance.
    struct PotentiometerEntry
    {
        std::size_t first_section_index = 0;   ///< Resistor-table index of the a-to-wiper section.
        std::size_t second_section_index = 0;  ///< Resistor-table index of the wiper-to-b section.
        double      total_ohms = 0.0;          ///< End-to-end track resistance.
    };

    /// @brief Control-thread description of a circuit: node indices and component values.
    ///
    /// Node 0 is ground and non-ground nodes are numbered from 1 upwards. This is a plain
    /// aggregate of fixed-size arrays so DkCircuit can own a copy and re-derive its matrices on
    /// the audio thread without touching the builder. Elements are stored as node pairs rather
    /// than as dense incidence matrices: every product in the DK derivation is a row or column
    /// difference, which a stamp loop over these pairs computes directly.
    ///
    /// The add methods assert on overflow or an out-of-range node, because they run on the
    /// control thread while a circuit is written by hand. DkCircuit::prepare re-validates and
    /// returns false instead, because it is also reachable from a preset load.
    /// @tparam S Reactive element (state) count.
    /// @tparam N Nonlinear branch count.
    /// @tparam I Input voltage source count.
    /// @tparam O Output probe count.
    template <std::size_t S, std::size_t N, std::size_t I, std::size_t O>
    struct DkNetlist
    {
        /// @brief Number of non-ground nodes; node indices run from 1 to this value.
        std::size_t node_count = 0;

        /// @brief Adds a resistor.
        /// @param a Positive terminal; 0 is ground.
        /// @param b Negative terminal; 0 is ground.
        /// @param ohms Resistance, strictly positive.
        void resistor(std::size_t a, std::size_t b, double ohms)
        {
            assert(resistor_count_ < kMaximumResistors);
            assert(a <= kMaximumNodes && b <= kMaximumNodes);
            assert(ohms > 0.0);
            resistors_[resistor_count_] = Branch{ a, b, ohms };
            ++resistor_count_;
        }

        /// @brief Adds a capacitor, discretised by the trapezoidal rule.
        /// @param a Positive terminal; 0 is ground.
        /// @param b Negative terminal; 0 is ground.
        /// @param farads Capacitance, strictly positive.
        void capacitor(std::size_t a, std::size_t b, double farads)
        {
            assert(reactive_count_ < S);
            assert(a <= kMaximumNodes && b <= kMaximumNodes);
            assert(farads > 0.0);
            reactive_[reactive_count_] = Branch{ a, b, farads };
            reactive_sign_[reactive_count_] = 1.0;
            ++reactive_count_;
        }

        /// @brief Adds an inductor, discretised by the trapezoidal rule.
        /// @param a Positive terminal; 0 is ground.
        /// @param b Negative terminal; 0 is ground.
        /// @param henries Inductance, strictly positive.
        void inductor(std::size_t a, std::size_t b, double henries)
        {
            assert(reactive_count_ < S);
            assert(a <= kMaximumNodes && b <= kMaximumNodes);
            assert(henries > 0.0);
            reactive_[reactive_count_] = Branch{ a, b, henries };
            reactive_sign_[reactive_count_] = -1.0;
            ++reactive_count_;
        }

        /// @brief Adds an ideal input voltage source.
        ///
        /// Its entry of the input vector u is the driven voltage V(a) minus V(b), in the order
        /// the sources were added.
        /// @param a Positive terminal; 0 is ground.
        /// @param b Negative terminal; 0 is ground.
        void voltage_source(std::size_t a, std::size_t b)
        {
            assert(source_count_ < I);
            assert(a <= kMaximumNodes && b <= kMaximumNodes);
            sources_[source_count_] = Branch{ a, b, 0.0 };
            ++source_count_;
        }

        /// @brief Adds a potentiometer as two resistor sections, a-to-wiper and wiper-to-b.
        ///
        /// Position 0 puts the wiper at @p a and position 1 puts it at @p b, so the a-to-wiper
        /// section is @p ohms times the position.
        /// @param a One end of the track; 0 is ground.
        /// @param wiper The wiper node; 0 is ground.
        /// @param b The other end of the track; 0 is ground.
        /// @param ohms End-to-end track resistance, strictly positive.
        /// @return The potentiometer's index, for DkCircuit::set_potentiometer.
        std::size_t potentiometer(std::size_t a, std::size_t wiper, std::size_t b, double ohms)
        {
            assert(potentiometer_count_ < kMaximumPotentiometers);
            assert(resistor_count_ + 2 <= kMaximumResistors);
            assert(ohms > 0.0);

            const std::size_t first = resistor_count_;
            resistor(a, wiper, ohms);
            const std::size_t second = resistor_count_;
            resistor(wiper, b, ohms);

            potentiometers_[potentiometer_count_] = PotentiometerEntry{ first, second, ohms };
            const std::size_t index = potentiometer_count_;
            ++potentiometer_count_;
            return index;
        }

        /// @brief Adds a nonlinear branch.
        ///
        /// The model receives v = V(a) minus V(b) as this branch's entry of the nonlinear
        /// voltage vector, and a positive returned current flows from @p a to @p b through the
        /// device.
        /// @param a Positive terminal; 0 is ground.
        /// @param b Negative terminal; 0 is ground.
        void nonlinear(std::size_t a, std::size_t b)
        {
            assert(nonlinear_count_ < N);
            assert(a <= kMaximumNodes && b <= kMaximumNodes);
            nonlinear_[nonlinear_count_] = Branch{ a, b, 0.0 };
            ++nonlinear_count_;
        }

        /// @brief Adds an output probe measuring V(a) minus V(b).
        /// @param a Positive terminal; 0 is ground.
        /// @param b Negative terminal; 0 is ground.
        void output(std::size_t a, std::size_t b)
        {
            assert(output_count_ < O);
            assert(a <= kMaximumNodes && b <= kMaximumNodes);
            outputs_[output_count_] = Branch{ a, b, 0.0 };
            ++output_count_;
        }

        std::array<Branch, kMaximumResistors> resistors_{};        ///< Resistor branches, pot sections included.
        std::size_t                           resistor_count_ = 0; ///< Populated entries of resistors_.
        std::array<Branch, S>                 reactive_{};         ///< Capacitors and inductors, in state order.
        std::array<double, S>                 reactive_sign_{};    ///< Z: +1 for a capacitor, -1 for an inductor.
        std::size_t                           reactive_count_ = 0; ///< Populated entries of reactive_.
        std::array<Branch, I>                 sources_{};          ///< Input sources, in input-vector order.
        std::size_t                           source_count_ = 0;   ///< Populated entries of sources_.
        std::array<Branch, N>                 nonlinear_{};        ///< Nonlinear branches, in nonlinear-port order.
        std::size_t                           nonlinear_count_ = 0; ///< Populated entries of nonlinear_.
        std::array<Branch, O>                 outputs_{};          ///< Output probes, in output-vector order.
        std::size_t                           output_count_ = 0;   ///< Populated entries of outputs_.

        std::array<PotentiometerEntry, kMaximumPotentiometers> potentiometers_{}; ///< Potentiometer tracks.
        std::size_t potentiometer_count_ = 0;                                     ///< Populated entries of potentiometers_.
    };
}
