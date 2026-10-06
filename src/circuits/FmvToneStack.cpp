/**************************************************************************/
/* FmvToneStack.cpp                                                       */
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
 * @file FmvToneStack.cpp
 * @brief The FMV stack's netlist and the DK block that solves it.
 * @author Mustafa Garip & Sushi Systems
 */

#include "SushiDSP/circuits/FmvToneStack.hpp"

#include <algorithm>
#include <array>
#include <cassert>

namespace SushiDSP
{
    namespace
    {
        /// @brief Node indices of the deck, with the source resistor's node folded out when absent.
        struct Nodes
        {
            std::size_t source;      ///< The driven node.
            std::size_t input;       ///< The stack's own input; the same node when the source is ideal.
            std::size_t treble_top;  ///< The treble cap's far end and the treble pot's a end.
            std::size_t output;      ///< The treble wiper.
            std::size_t bass_top;    ///< Treble pot's b end, the bass cap's far end, the bass pot's a end.
            std::size_t slope;       ///< The slope resistor's far end, feeding both lower caps.
            std::size_t middle_top;  ///< The bass rheostat's wiper and the middle rheostat's a end.
            std::size_t count;       ///< Number of non-ground nodes.
        };

        /// @brief Returns the deck's node indices for a termination.
        /// @param has_source_resistance Whether a resistor sits between the source and the stack.
        Nodes nodes_for(bool has_source_resistance)
        {
            const std::size_t shift = has_source_resistance ? 0u : 1u;
            return Nodes{ 1u, has_source_resistance ? 2u : 1u, 3u - shift, 4u - shift,
                          5u - shift, 6u - shift, 7u - shift, 7u - shift };
        }
    }

    FmvToneStack::FmvToneStack(double sample_rate, const FmvComponents& components,
                               const FmvSourceLoad& termination)
        : sample_rate_(sample_rate), components_(components), termination_(termination)
    {
        build();
    }

    void FmvToneStack::build()
    {
        const bool has_source_resistance = termination_.source_resistance_ohms > 0.0f;
        const Nodes node = nodes_for(has_source_resistance);

        dk::DkNetlist<kStateCount, 0, 1, 1> netlist;
        netlist.node_count = node.count;
        netlist.voltage_source(node.source, 0);
        if (has_source_resistance)
        {
            netlist.resistor(node.source, node.input, termination_.source_resistance_ohms);
        }

        netlist.capacitor(node.input, node.treble_top, components_.treble_capacitor_farads);
        netlist.resistor(node.input, node.slope, components_.slope_resistor_ohms);
        netlist.capacitor(node.slope, node.bass_top, components_.bass_capacitor_farads);
        netlist.capacitor(node.slope, node.middle_top, components_.middle_capacitor_farads);

        // The treble pot's DK position is one minus the panel position (src/README.md).
        treble_pot_ = netlist.potentiometer(node.treble_top, node.output, node.bass_top,
                                           components_.treble_potentiometer_ohms);

        // Both lower controls are rheostats; the dead section stamps nothing (src/README.md).
        bass_pot_ = netlist.potentiometer(node.bass_top, node.middle_top, node.middle_top,
                                         components_.bass_potentiometer_ohms);
        middle_pot_ = netlist.potentiometer(node.middle_top, 0, 0,
                                           components_.middle_potentiometer_ohms);

        if (termination_.load_resistance_ohms > 0.0f)
        {
            netlist.resistor(node.output, 0, termination_.load_resistance_ohms);
        }
        netlist.output(node.output, 0);

        const bool prepared = circuit_.prepare(netlist, sample_rate_);
        assert(prepared);
        static_cast<void>(prepared);

        circuit_.set_potentiometer(treble_pot_, 1.0 - static_cast<double>(treble_));
        circuit_.set_potentiometer(bass_pot_, static_cast<double>(bass_));
        circuit_.set_potentiometer(middle_pot_, static_cast<double>(middle_));
        circuit_.update_matrices_if_dirty();
    }

    void FmvToneStack::set_bass(float normalized)
    {
        bass_ = std::clamp(normalized, 0.0f, 1.0f);
        circuit_.set_potentiometer(bass_pot_, static_cast<double>(bass_));
        circuit_.update_matrices_if_dirty();
    }

    void FmvToneStack::set_middle(float normalized)
    {
        middle_ = std::clamp(normalized, 0.0f, 1.0f);
        circuit_.set_potentiometer(middle_pot_, static_cast<double>(middle_));
        circuit_.update_matrices_if_dirty();
    }

    void FmvToneStack::set_treble(float normalized)
    {
        treble_ = std::clamp(normalized, 0.0f, 1.0f);
        circuit_.set_potentiometer(treble_pot_, 1.0 - static_cast<double>(treble_));
        circuit_.update_matrices_if_dirty();
    }

    float FmvToneStack::process_sample(float input)
    {
        const std::array<double, 1> inputs{ static_cast<double>(input) };
        std::array<double, 1> outputs{};
        circuit_.process_sample(port_, inputs, outputs);
        return static_cast<float>(outputs[0]);
    }

    void FmvToneStack::reset()
    {
        circuit_.reset();
    }
}
