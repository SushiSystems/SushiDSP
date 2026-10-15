/**************************************************************************/
/* test_rt_safety.cpp                                                     */
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
 * @file test_rt_safety.cpp
 * @brief Tests that the audio-thread paths of DkCircuit perform no heap allocation.
 * @author Mustafa Garip
 */

// _USE_MATH_DEFINES precedes every include so that MSVC's <cmath> declares M_PI.
#define _USE_MATH_DEFINES

#include <array>
#include <cmath>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/allocation_trap/allocation_trap.hpp>
#include <SushiDSP/foundation/math/dk/dk_circuit.hpp>
#include <SushiDSP/foundation/math/dk/dk_netlist.hpp>
#include <SushiDSP/foundation/circuit_blocks/jcm_v1a_netlist.hpp>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{
    // The netlist lives in modules/foundation/circuit_blocks/include/SushiDSP/foundation/circuit_blocks/jcm_v1a_netlist.hpp. See tests/foundation/math/dk/test_dk_circuit/README.md.
    using namespace SushiDSP::test_circuits;

    using DkStageCircuit = SushiDSP::dk::DkCircuit<6, 2, 2, 1>;
    using DkStageNetlist = SushiDSP::dk::DkNetlist<6, 2, 2, 1>;
} // namespace

TEST(Small_DkCircuit, ProcessPerformsNoHeapAllocation)
{
    DkStageNetlist netlist;
    build_jcm_v1a(netlist);

    DkStageCircuit circuit;
    TriodeStagePort port;
    ASSERT_TRUE(circuit.prepare(netlist, 48000.0));

    const std::array<double, 2> silence{0.0, K_SUPPLY_VOLTS};
    std::array<double, 1> outputs{};
    for (int sample = 0; sample < 4800; ++sample)
    {
        circuit.process_sample(port, silence, outputs);
    }

    SushiDSP::test::AllocationTrap trap;
    trap.arm();

    for (int sample = 0; sample < 256; ++sample)
    {
        const double drive =
            0.2 * std::sin(2.0 * M_PI * 220.0 * static_cast<double>(sample) / 48000.0);
        const std::array<double, 2> inputs{drive, K_SUPPLY_VOLTS};
        circuit.process_sample(port, inputs, outputs);
    }

    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
    EXPECT_EQ(circuit.nan_fallback_samples(), 0u);
}

TEST(Small_DkCircuit, PotentiometerUpdatePerformsNoHeapAllocation)
{
    // A pot move rebuilds the DK matrices on the audio thread. See tests/foundation/math/dk/test_dk_circuit/README.md.
    DkStageNetlist netlist;
    const std::size_t pot_index = build_jcm_v1a(netlist);

    DkStageCircuit circuit;
    TriodeStagePort port;
    ASSERT_TRUE(circuit.prepare(netlist, 48000.0));

    const std::array<double, 2> silence{0.0, K_SUPPLY_VOLTS};
    std::array<double, 1> outputs{};

    SushiDSP::test::AllocationTrap trap;

    int rebuilt_blocks = 0;
    for (int block = 0; block < 16; ++block)
    {
        const double position = 0.1 + 0.05 * static_cast<double>(block);

        trap.arm();

        circuit.set_potentiometer(pot_index, position);
        rebuilt_blocks += circuit.update_matrices_if_dirty() ? 1 : 0;
        for (int sample = 0; sample < 64; ++sample)
        {
            circuit.process_sample(port, silence, outputs);
        }

        trap.disarm();
    }

    EXPECT_EQ(rebuilt_blocks, 16);
    EXPECT_EQ(trap.count(), 0);
}
