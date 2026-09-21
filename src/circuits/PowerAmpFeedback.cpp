/**************************************************************************/
/* PowerAmpFeedback.cpp                                                   */
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
 * @file PowerAmpFeedback.cpp
 * @brief The feedback leg's netlist and the DK block that solves it.
 * @author Mustafa Garip & Sushi Systems
 */

#include "SushiDSP/circuits/PowerAmpFeedback.hpp"

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
            static constexpr std::size_t kTap = 1;      ///< The speaker tap, the driven node.
            static constexpr std::size_t kPresence = 2; ///< Presence wiper, above the fixed resistor.
            static constexpr std::size_t kShaped = 3;   ///< Below that resistor, into the shaping pair.
            static constexpr std::size_t kSplit = 4;    ///< Where the tail feed and the divider part.
            static constexpr std::size_t kDivider = 5;  ///< The divider's output, above its ground leg.
            static constexpr std::size_t kTail = 6;     ///< The inverter's tail node.
            static constexpr std::size_t kGrid = 7;     ///< The inverter's second grid, the output.
            static constexpr std::size_t kCount = 7;    ///< Number of non-ground nodes.
        };

        /// @brief The tail impedance the grid's source resistance is worked out against, a valve
        /// pair's cathodes seen through their shared-cathode resistor.
        constexpr float kTailNominalOhms = 1068.5f;
    }

    float feedback_tail_resistance_ohms(const PowerAmpFeedbackComponents& components)
    {
        return components.tail_feed_resistor_ohms + components.divider_series_resistor_ohms
            + components.divider_shunt_resistor_ohms;
    }

    float feedback_grid_source_resistance_ohms(const PowerAmpFeedbackComponents& components)
    {
        const auto parallel = [](float a, float b) { return a * b / (a + b); };
        const float above = 0.5f * components.presence_potentiometer_ohms
            + components.tap_series_resistor_ohms + components.shaping_resistor_ohms;
        const float split = parallel(components.tail_feed_resistor_ohms + kTailNominalOhms, above);
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
        dk::DkNetlist<kStateCount, 0, 1, 1> netlist;
        netlist.node_count = Nodes::kCount;
        netlist.voltage_source(Nodes::kTap, 0);

        // Presence is a rheostat: its dead wiper-to-b section stamps nothing, one node at both ends.
        presence_pot_ = netlist.potentiometer(Nodes::kTap, Nodes::kPresence, Nodes::kPresence,
                                              components_.presence_potentiometer_ohms);
        netlist.resistor(Nodes::kPresence, Nodes::kShaped, components_.tap_series_resistor_ohms);

        netlist.resistor(Nodes::kShaped, Nodes::kSplit, components_.shaping_resistor_ohms);
        netlist.capacitor(Nodes::kShaped, Nodes::kSplit, components_.shaping_capacitor_farads);

        netlist.resistor(Nodes::kSplit, Nodes::kTail, components_.tail_feed_resistor_ohms);
        netlist.resistor(Nodes::kSplit, Nodes::kDivider, components_.divider_series_resistor_ohms);
        netlist.resistor(Nodes::kDivider, 0, components_.divider_shunt_resistor_ohms);
        netlist.capacitor(Nodes::kDivider, 0, components_.divider_shunt_capacitor_farads);

        netlist.capacitor(Nodes::kDivider, Nodes::kGrid, components_.coupling_capacitor_farads);
        netlist.resistor(Nodes::kGrid, Nodes::kTail, components_.grid_leak_resistor_ohms);

        if (termination_.tail_impedance_ohms > 0.0f)
        {
            netlist.resistor(Nodes::kTail, 0, termination_.tail_impedance_ohms);
        }
        netlist.output(Nodes::kGrid, 0);

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
        const std::array<double, 1> inputs{ static_cast<double>(tap_voltage) };
        std::array<double, 1> outputs{};
        circuit_.process_sample(port_, inputs, outputs);
        return static_cast<float>(outputs[0]);
    }

    void PowerAmpFeedback::reset()
    {
        circuit_.reset();
    }
}
