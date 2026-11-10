/**************************************************************************/
/* test_op_amp.cpp                                                        */
/**************************************************************************/
/*                          This file is part of:                         */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                         https://sushisystems.io                        */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License"); you    */
/* may not use this file except in compliance with the License. You may   */
/* obtain a copy of the License at                                        */
/*                                                                        */
/*     https://www.apache.org/licenses/LICENSE-2.0                        */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

/**
 * @file test_op_amp.cpp
 * @brief Tests for the ideal op-amp nullor element in DkNetlist.
 * @author Mustafa Garip & Sushi Systems
 */

#include <array>
#include <cmath>
#include <cstddef>
#include <string>

#include <gtest/gtest.h>

#include "support/tolerance.hpp"

#include <SushiDSP/foundation/math/dk/dk_circuit.hpp>
#include <SushiDSP/foundation/math/dk/dk_netlist.hpp>
#include <SushiDSP/foundation/math/dk/linear_port.hpp>

namespace
{
    /// The rounded operations an ideal op-amp stage's LU solve may spend on one output: the augmented nodal system
    /// has order five or less, and its condition number follows the 10:1 to 11:1 resistor ratios, so 100 operations
    /// of u cover them with the threshold of 16 on top.
    constexpr std::size_t K_SOLVE_OPERATIONS = 100;

    constexpr double K_SAMPLE_RATE = 48000.0;

    using StaticCircuit = SushiDSP::dk::DkCircuit<0, 0, 1, 1>;
    using StaticNetlist = SushiDSP::dk::DkNetlist<0, 0, 1, 1>;

    /// @brief Holds an input voltage of a stage and the name its row is reported under.
    struct InputRow
    {
            const char* name;
            double volts;
    };

    // V(3) = -(Rf/Rin) V(1) = -10 V(1) at every frequency. See tests/foundation/math/dk/test_dk_circuit/README.md.
    const InputRow kInvertingInputs[] = {
        {"MinusThree", -3.0}, {"MinusQuarter", -0.25}, {"Zero", 0.0}, {"Quarter", 0.25}, {"Three", 3.0},
    };

    /// @brief Builds the inverting stage: 10 kOhm in, 100 kOhm feedback, the op-amp's input at node 2.
    void build_inverting(StaticNetlist& netlist)
    {
        netlist.node_count = 3;
        netlist.voltage_source(1, 0);
        netlist.resistor(1, 2, 10.0e3);
        netlist.resistor(2, 3, 100.0e3);
        netlist.nullor(0, 2, 3, 0);
        netlist.output(3, 0);
    }

    /// @brief Returns the output of a prepared static stage for one input.
    double static_output(StaticCircuit& circuit, double input)
    {
        SushiDSP::dk::LinearPort nonlinear;
        std::array<double, 1> outputs{};
        circuit.process_sample(nonlinear, std::array<double, 1>{input}, outputs);
        return outputs[0];
    }

    std::string input_name(const ::testing::TestParamInfo<InputRow>& info)
    {
        return std::string(info.param.name);
    }
} // namespace

class Small_DkCircuitInvertingOpAmp : public ::testing::TestWithParam<InputRow>
{
};

TEST_P(Small_DkCircuitInvertingOpAmp, HasGainMinusRfOverRin)
{
    StaticNetlist netlist;
    build_inverting(netlist);
    StaticCircuit circuit;
    ASSERT_TRUE(circuit.prepare(netlist, K_SAMPLE_RATE));

    const double output = static_output(circuit, GetParam().volts);
    EXPECT_TRUE(TestSupport::is_within(
        TestSupport::measure_relative_error<double>(output, -10.0 * GetParam().volts, K_SOLVE_OPERATIONS)));
    EXPECT_EQ(circuit.nan_fallback_samples(), 0u);
}

INSTANTIATE_TEST_SUITE_P(Inputs, Small_DkCircuitInvertingOpAmp, ::testing::ValuesIn(kInvertingInputs), input_name);

TEST(Small_DkCircuit, IdealOpAmpNonInvertingAmplifierHasGainOnePlusRfOverRg)
{
    // 1 + 51000/4700 = 11.851063829787234 exactly. See tests/foundation/math/dk/test_dk_circuit/README.md.
    StaticNetlist netlist;
    netlist.node_count = 3;
    netlist.voltage_source(1, 0);
    netlist.resistor(3, 2, 51.0e3);
    netlist.resistor(2, 0, 4700.0);
    netlist.nullor(1, 2, 3, 0);
    netlist.output(3, 0);

    StaticCircuit circuit;
    ASSERT_TRUE(circuit.prepare(netlist, K_SAMPLE_RATE));

    const double expected = 1.0 + 51.0e3 / 4700.0;
    EXPECT_TRUE(TestSupport::is_within(
        TestSupport::measure_relative_error<double>(static_output(circuit, 1.0), expected, K_SOLVE_OPERATIONS)));
}

TEST(Small_DkCircuit, IdealOpAmpVoltageFollowerPassesItsInputThrough)
{
    // The degenerate case: the sensed node and the driven node are the same one.
    StaticNetlist netlist;
    netlist.node_count = 2;
    netlist.voltage_source(1, 0);
    netlist.resistor(2, 0, 10.0e3);
    netlist.nullor(1, 2, 2, 0);
    netlist.output(2, 0);

    StaticCircuit circuit;
    ASSERT_TRUE(circuit.prepare(netlist, K_SAMPLE_RATE));

    EXPECT_TRUE(TestSupport::is_within(
        TestSupport::measure_relative_error<double>(static_output(circuit, 0.7), 0.7, K_SOLVE_OPERATIONS)));
}

TEST(Small_DkCircuit, RejectsAnOpAmpWithNoFeedbackPath)
{
    // Without a feedback path the system is rank deficient. See tests/foundation/math/dk/test_dk_circuit/README.md.
    StaticNetlist netlist;
    netlist.node_count = 3;
    netlist.voltage_source(1, 0);
    netlist.resistor(1, 2, 10.0e3);
    netlist.resistor(3, 0, 10.0e3);
    netlist.nullor(0, 2, 3, 0);
    netlist.output(3, 0);

    StaticCircuit circuit;
    EXPECT_FALSE(circuit.prepare(netlist, K_SAMPLE_RATE));
}
