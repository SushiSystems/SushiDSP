/**************************************************************************/
/* test_phase_inverter_presets.cpp                                        */
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
 * @file test_phase_inverter_presets.cpp
 * @brief The single-triode inverter preset's operating point and the stage it drives.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include "SushiDSP/circuits/stages/PhaseInverterPresets.hpp"
#include "SushiDSP/circuits/stages/TriodeStage.hpp"

TEST(Unit_Dsp, Triode12AX7Inverter82kPlateHoldsItsSolvedOperatingPoint)
{
    const SushiDSP::SingleTriodeInverter& inverter = SushiDSP::kTriode12AX7Inverter82kPlate;
    EXPECT_FLOAT_EQ(inverter.cathode_bias_volts, -1.346012f);
    EXPECT_FLOAT_EQ(inverter.gain, 61.927402f);
    EXPECT_FLOAT_EQ(inverter.input_degeneration, 1.888775f);
    EXPECT_FLOAT_EQ(inverter.plate_swing.headroom_to_supply_volts, 117.418041f);
    EXPECT_FLOAT_EQ(inverter.plate_swing.headroom_to_bottom_volts, 89.548144f);
}

TEST(Unit_Dsp, Triode12AX7Inverter82kPlateRestsBelowItsCathodeAndDegeneratesItsInput)
{
    const SushiDSP::SingleTriodeInverter& inverter = SushiDSP::kTriode12AX7Inverter82kPlate;
    EXPECT_LT(inverter.cathode_bias_volts, 0.0f);
    EXPECT_GT(inverter.gain, 0.0f);
    EXPECT_GT(inverter.input_degeneration, 1.0f);
    EXPECT_GT(inverter.plate_swing.headroom_to_supply_volts, 0.0f);
    EXPECT_GT(inverter.plate_swing.headroom_to_bottom_volts, 0.0f);
}

TEST(Unit_Dsp, Triode12AX7Inverter82kPlateDrivesATriodeStageThatInvertsAndBounds)
{
    const SushiDSP::SingleTriodeInverter& inverter = SushiDSP::kTriode12AX7Inverter82kPlate;
    SushiDSP::TriodeStage stage(10000.0f, 1000000.0f, inverter.cathode_bias_volts, inverter.gain,
                                SushiDSP::kTriode12AX7GridConduction, inverter.plate_swing);
    EXPECT_LT(stage.process_sample(0.5f), 0.0f);
    stage.reset();
    EXPECT_GT(stage.process_sample(-0.5f), 0.0f);
    stage.reset();
    EXPECT_LE(stage.process_sample(1000.0f), inverter.plate_swing.headroom_to_bottom_volts);
}
