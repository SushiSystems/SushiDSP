/**************************************************************************/
/* power_amp_feedback.hpp                                                 */
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
 * @file power_amp_feedback.hpp
 * @brief The power amplifier's feedback leg as a DK netlist, presence pot a rheostat.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <SushiDSP/foundation/math/dk/dk_circuit.hpp>
#include <SushiDSP/foundation/math/dk/dk_netlist.hpp>
#include <SushiDSP/foundation/math/dk/linear_port.hpp>

namespace SushiDSP
{
    /// @brief Component values of a feedback leg from the output transformer's speaker tap.
    struct PowerAmpFeedbackComponents
    {
            float
                presence_potentiometer_ohms; ///< The presence pot, a rheostat in series with the tap.
            float tap_series_resistor_ohms;     ///< The fixed resistor below the presence pot.
            float shaping_resistor_ohms;        ///< The series resistor its own capacitor bridges.
            float shaping_capacitor_farads;     ///< The capacitor across that resistor.
            float tail_feed_resistor_ohms;      ///< From the shaped node to the inverter's tail.
            float divider_series_resistor_ohms; ///< From the shaped node into the divider.
            float divider_shunt_resistor_ohms;  ///< The divider's leg to ground.
            float divider_shunt_capacitor_farads; ///< The capacitor across that leg.
            float coupling_capacitor_farads;      ///< From the divider into the driven grid.
            float grid_leak_resistor_ohms;        ///< That grid's leak, returning to the tail.
    };

    /// @brief What the inverter presents where the leg reaches its tail.
    ///
    /// The leg is a passive divider, so what hangs on the tail node changes its response. Zero
    /// leaves the tail open, which is the leg on its own.
    struct PowerAmpFeedbackTermination
    {
            float tail_impedance_ohms =
                0.0f; ///< Impedance from the tail node to ground; 0 is open.
    };

    /// @brief The Mark IIC+'s values, read off jrb32's traced schematic
    /// (docs/agent/2026_09_20_V0_1_0_RELEASE/REPORT.md).
    inline constexpr PowerAmpFeedbackComponents K_MARK2C_FEEDBACK{
        250e3f, 1.5e3f, 56e3f, 4.7e-9f, 22e3f, 1.5e3f, 3.3e3f, 47e-9f, 0.1e-6f, 150e3f};

    /// @brief Returns the resistance the inverter's tail sees to ground through the leg.
    ///
    /// The leg carries the tail's own return, so this is what a long-tailed pair built beside it
    /// takes as its tail resistance; passing anything else makes the two disagree.
    /// @param components The leg's component values.
    float feedback_tail_resistance_ohms(const PowerAmpFeedbackComponents& components);

    /// @brief Returns the resistance the driven grid is fed from, looking back into the leg.
    ///
    /// This is the Thevenin resistance at the divider, which the grid reaches through a coupling
    /// capacitor low against it across the band. It moves by about a hundred ohms over the presence
    /// pot's whole travel, so it is taken at the pot's midpoint and treated as fixed.
    /// @param components The leg's component values.
    float feedback_grid_source_resistance_ohms(const PowerAmpFeedbackComponents& components);

    /// @brief The feedback leg from the speaker tap to the phase inverter's second grid.
    ///
    /// Two capacitors pull the leg's response in opposite directions: the one across the shaping
    /// resistor bridges it above its corner, which feeds back more treble, while the one across
    /// the divider's ground leg shunts it above its own corner, which feeds back less. Which wins
    /// where is what the solve answers, and it is why the leg is not a shelf. The presence pot
    /// sits in series with the tap, so opening it removes feedback rather than shunting it away.
    class PowerAmpFeedback
    {
        public:
            /// @param sample_rate The rate the leg runs at, in hertz.
            /// @param components The leg's component values.
            /// @param termination What the inverter presents at the tail node.
            /// @throws std::runtime_error if the DK derivation fails at @p sample_rate.
            PowerAmpFeedback(double sample_rate, const PowerAmpFeedbackComponents& components,
                             const PowerAmpFeedbackTermination& termination = {});

            /// @brief Sets the presence pot's position and re-derives the matrices.
            /// @param normalized Presence position in [0, 1]; 0 is the whole pot shorted out, so the
            /// deepest feedback and the darkest tone. Values outside are clamped.
            void set_presence(float normalized);

            /// @brief Processes one sample. Audio-thread safe.
            /// @param tap_voltage The voltage at the speaker tap for this sample.
            /// @return The voltage the leg puts on the inverter's second grid.
            float process_sample(float tap_voltage);

            /// @brief Clears the reactive state.
            void reset();

        private:
            /// @brief Number of capacitors, and so of DK states.
            static constexpr std::size_t K_STATE_COUNT = 3;

            /// @brief Builds the netlist from the components and the termination.
            void build();

            double sample_rate_;
            PowerAmpFeedbackComponents components_;
            PowerAmpFeedbackTermination termination_;
            float presence_ = 0.5f;
            std::size_t presence_pot_ = 0; ///< DK index of the presence rheostat.
            dk::LinearPort port_{};
            dk::DkCircuit<K_STATE_COUNT, 0, 1, 1> circuit_{};
    };
} // namespace SushiDSP
