/**************************************************************************/
/* test_dk_circuit.cpp                                                    */
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

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>

#include "SushiDSP/math/dk/DkCircuit.hpp"
#include "SushiDSP/math/dk/DkNetlist.hpp"

namespace
{
    /// A deliberately trivial nonlinear port: a linear conductance. It exercises the glue
    /// contract and the Newton path without depending on any circuit model, which keeps this
    /// instantiation gate independent of Tasks 2 to 4.
    template <std::size_t N>
    struct LinearPort
    {
        double conductance = 1e-6;

        void currents(const std::array<double, N>& v, std::array<double, N>& i) const
        {
            for (std::size_t index = 0; index < N; ++index)
            {
                i[index] = conductance * v[index];
            }
        }

        void jacobian(const std::array<double, N>& v, std::array<std::array<double, N>, N>& j) const
        {
            static_cast<void>(v);
            for (std::size_t index = 0; index < N; ++index)
            {
                j[index][index] = conductance;
            }
        }
    };
}

// Compile-and-smoke gate for the three DkCircuit shapes D6c and D6d instantiate. Tasks 6 and 7
// own the numerical validation; this case only proves every shape compiles, prepares and steps.
TEST(Unit_Dsp, DkCircuitInstantiatesEveryShapeD6cAndD6dNeed)
{
    constexpr double kSampleRate = 48000.0;

    {
        // Diode-clipper shape: source - R - node - nonlinear - ground.
        SushiDSP::dk::DkNetlist<0, 1, 1, 1> netlist;
        netlist.node_count = 2;
        netlist.voltage_source(1, 0);
        netlist.resistor(1, 2, 2.2e3);
        netlist.nonlinear(2, 0);
        netlist.output(2, 0);

        SushiDSP::dk::DkCircuit<0, 1, 1, 1> circuit;
        ASSERT_TRUE(circuit.prepare(netlist, kSampleRate));

        LinearPort<1>         port;
        std::array<double, 1> inputs{ 0.5 };
        std::array<double, 1> outputs{};
        circuit.process_sample(port, inputs, outputs);

        EXPECT_TRUE(std::isfinite(outputs[0]));
        EXPECT_EQ(circuit.total_samples(), 1u);
        EXPECT_EQ(circuit.non_converged_samples(), 0u);
        EXPECT_EQ(circuit.nan_fallback_samples(), 0u);
        EXPECT_TRUE(std::isfinite(circuit.nonlinear_currents()[0]));
        EXPECT_TRUE(std::isfinite(circuit.nonlinear_voltages()[0]));
        EXPECT_EQ(circuit.states().size(), 0u);
        EXPECT_LE(circuit.maximum_iterations_seen(), 8u);

        circuit.set_state_limit(1e3);
        circuit.reset_statistics();
        circuit.reset();
        EXPECT_EQ(circuit.total_samples(), 0u);
    }

    {
        // Purely linear shape: two cascaded RC sections, no Newton path at all.
        SushiDSP::dk::DkNetlist<2, 0, 1, 1> netlist;
        netlist.node_count = 3;
        netlist.voltage_source(1, 0);
        netlist.resistor(1, 2, 1.0e4);
        netlist.capacitor(2, 0, 1.0e-8);
        netlist.resistor(2, 3, 1.0e5);
        netlist.capacitor(3, 0, 1.0e-9);
        netlist.output(3, 0);

        SushiDSP::dk::DkCircuit<2, 0, 1, 1> circuit;
        ASSERT_TRUE(circuit.prepare(netlist, kSampleRate));

        LinearPort<0>         port;
        std::array<double, 1> inputs{ 1.0 };
        std::array<double, 1> outputs{};
        for (int sample = 0; sample < 8; ++sample)
        {
            circuit.process_sample(port, inputs, outputs);
        }

        EXPECT_TRUE(std::isfinite(outputs[0]));
        EXPECT_EQ(circuit.total_samples(), 8u);
        EXPECT_EQ(circuit.nan_fallback_samples(), 0u);
        EXPECT_EQ(circuit.nonlinear_currents().size(), 0u);
        EXPECT_EQ(circuit.nonlinear_voltages().size(), 0u);
        EXPECT_TRUE(std::isfinite(circuit.states()[0]));
        circuit.reset();
    }

    {
        // The widest D6d-style shape: four states, a two-branch nonlinear port, two sources
        // (signal plus supply rail), one probe, and a potentiometer to drive re-derivation.
        SushiDSP::dk::DkNetlist<4, 2, 2, 1> netlist;
        netlist.node_count = 5;
        netlist.voltage_source(1, 0);
        netlist.voltage_source(4, 0);
        netlist.resistor(1, 2, 1.0e4);
        netlist.capacitor(1, 2, 1.0e-10);
        netlist.capacitor(2, 0, 1.0e-8);
        netlist.nonlinear(2, 0);
        netlist.resistor(2, 3, 1.0e5);
        netlist.capacitor(3, 0, 1.0e-9);
        netlist.nonlinear(3, 0);
        netlist.resistor(3, 4, 1.0e4);
        netlist.capacitor(3, 4, 1.0e-9);
        const std::size_t volume = netlist.potentiometer(3, 5, 0, 1.0e5);
        netlist.output(5, 0);

        SushiDSP::dk::DkCircuit<4, 2, 2, 1> circuit;
        ASSERT_TRUE(circuit.prepare(netlist, kSampleRate));

        EXPECT_FALSE(circuit.update_matrices_if_dirty());
        circuit.set_potentiometer(volume, 0.5);
        EXPECT_FALSE(circuit.update_matrices_if_dirty());
        circuit.set_potentiometer(volume, 0.25);
        EXPECT_TRUE(circuit.update_matrices_if_dirty());
        circuit.set_potentiometer(volume, 0.0);
        EXPECT_TRUE(circuit.update_matrices_if_dirty());
        circuit.set_potentiometer(volume, 1.0);
        EXPECT_TRUE(circuit.update_matrices_if_dirty());

        LinearPort<2>         port;
        std::array<double, 2> inputs{ 0.1, 250.0 };
        std::array<double, 1> outputs{};
        for (int sample = 0; sample < 8; ++sample)
        {
            circuit.process_sample(port, inputs, outputs);
        }

        EXPECT_TRUE(std::isfinite(outputs[0]));
        EXPECT_EQ(circuit.total_samples(), 8u);
        EXPECT_EQ(circuit.nan_fallback_samples(), 0u);
        EXPECT_EQ(circuit.non_converged_samples(), 0u);
        for (std::size_t index = 0; index < 4; ++index)
        {
            EXPECT_TRUE(std::isfinite(circuit.states()[index]));
        }
        circuit.set_state_limit(1e4);
        circuit.reset_statistics();
        circuit.reset();
    }
}
