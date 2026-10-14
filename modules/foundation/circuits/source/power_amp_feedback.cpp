/**************************************************************************/
/* power_amp_feedback.cpp                                                 */
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
 * @file power_amp_feedback.cpp
 * @brief The feedback leg's netlist and the DK block that solves it.
 * @author Mustafa Garip & Sushi Systems
 */

#include <SushiDSP/foundation/circuits/power_amp_feedback.hpp>

#include <algorithm>
#include <array>
#include <cassert>

namespace SushiDSP
{
    namespace
    {
        /// @brief Node indices of the leg.
        struct Nodes
        {
                static constexpr std::size_t K_TAP = 1; ///< The speaker tap, the driven node.
                static constexpr std::size_t K_PRESENCE =
                    2; ///< Presence wiper, above the fixed resistor.
                static constexpr std::size_t K_SHAPED =
                    3; ///< Below that resistor, into the shaping pair.
                static constexpr std::size_t K_SPLIT =
                    4; ///< Where the tail feed and the divider part.
                static constexpr std::size_t K_DIVIDER =
                    5; ///< The divider's output, above its ground leg.
                static constexpr std::size_t K_TAIL = 6; ///< The inverter's tail node.
                static constexpr std::size_t K_GRID =
                    7; ///< The inverter's second grid, the output.
                static constexpr std::size_t K_COUNT = 7; ///< Number of non-ground nodes.
        };

        /// @brief The tail impedance the grid's source resistance is worked out against, a valve
        /// pair's cathodes seen through their shared-cathode resistor.
        constexpr float K_TAIL_NOMINAL_OHMS = 1068.5f;
    } // namespace

    float feedback_tail_resistance_ohms(const PowerAmpFeedbackComponents& components)
    {
        return components.tail_feed_resistor_ohms + components.divider_series_resistor_ohms +
               components.divider_shunt_resistor_ohms;
    }

    float feedback_grid_source_resistance_ohms(const PowerAmpFeedbackComponents& components)
    {
        const auto parallel = [](float a, float b)
        {
            return a * b / (a + b);
        };
        const float above = 0.5f * components.presence_potentiometer_ohms +
                            components.tap_series_resistor_ohms + components.shaping_resistor_ohms;
        const float split =
            parallel(components.tail_feed_resistor_ohms + K_TAIL_NOMINAL_OHMS, above);
        return parallel(components.divider_shunt_resistor_ohms,
                        components.divider_series_resistor_ohms + split);
    }

    PowerAmpFeedback::PowerAmpFeedback(double sample_rate,
                                       const PowerAmpFeedbackComponents& components,
                                       const PowerAmpFeedbackTermination& termination)
        : sample_rate_(sample_rate), components_(components), termination_(termination)
    {
        build();
    }

    void PowerAmpFeedback::build()
    {
        dk::DkNetlist<K_STATE_COUNT, 0, 1, 1> netlist;
        netlist.node_count = Nodes::K_COUNT;
        netlist.voltage_source(Nodes::K_TAP, 0);

        // Presence is a rheostat: its dead wiper-to-b section stamps nothing, one node at both ends.
        presence_pot_ = netlist.potentiometer(Nodes::K_TAP, Nodes::K_PRESENCE, Nodes::K_PRESENCE,
                                              components_.presence_potentiometer_ohms);
        netlist.resistor(Nodes::K_PRESENCE, Nodes::K_SHAPED, components_.tap_series_resistor_ohms);

        netlist.resistor(Nodes::K_SHAPED, Nodes::K_SPLIT, components_.shaping_resistor_ohms);
        netlist.capacitor(Nodes::K_SHAPED, Nodes::K_SPLIT, components_.shaping_capacitor_farads);

        netlist.resistor(Nodes::K_SPLIT, Nodes::K_TAIL, components_.tail_feed_resistor_ohms);
        netlist.resistor(Nodes::K_SPLIT, Nodes::K_DIVIDER,
                         components_.divider_series_resistor_ohms);
        netlist.resistor(Nodes::K_DIVIDER, 0, components_.divider_shunt_resistor_ohms);
        netlist.capacitor(Nodes::K_DIVIDER, 0, components_.divider_shunt_capacitor_farads);

        netlist.capacitor(Nodes::K_DIVIDER, Nodes::K_GRID, components_.coupling_capacitor_farads);
        netlist.resistor(Nodes::K_GRID, Nodes::K_TAIL, components_.grid_leak_resistor_ohms);

        if (termination_.tail_impedance_ohms > 0.0f)
        {
            netlist.resistor(Nodes::K_TAIL, 0, termination_.tail_impedance_ohms);
        }
        netlist.output(Nodes::K_GRID, 0);

        const bool prepared = circuit_.prepare(netlist, sample_rate_);
        assert(prepared);
        static_cast<void>(prepared);

        circuit_.set_potentiometer(presence_pot_, static_cast<double>(presence_));
        circuit_.update_matrices_if_dirty();
    }

    void PowerAmpFeedback::set_presence(float normalized)
    {
        presence_ = std::clamp(normalized, 0.0f, 1.0f);
        circuit_.set_potentiometer(presence_pot_, static_cast<double>(presence_));
        circuit_.update_matrices_if_dirty();
    }

    float PowerAmpFeedback::process_sample(float tap_voltage)
    {
        const std::array<double, 1> inputs{static_cast<double>(tap_voltage)};
        std::array<double, 1> outputs{};
        circuit_.process_sample(port_, inputs, outputs);
        return static_cast<float>(outputs[0]);
    }

    void PowerAmpFeedback::reset()
    {
        circuit_.reset();
    }
} // namespace SushiDSP
