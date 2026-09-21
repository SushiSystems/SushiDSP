/**************************************************************************/
/* PowerTubePresets.hpp                                                   */
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

#include "SushiDSP/circuits/models/PentodeEL34.hpp"
#include "SushiDSP/circuits/stages/GridConductionLaw.hpp"
#include "SushiDSP/circuits/stages/PlateSwing.hpp"

namespace SushiDSP
{
    /// @brief The power tube's grid law, read from PentodeEL34 so the stage and the DK model agree.
    inline constexpr GridConductionLaw kPentodeEL34GridConduction{
        static_cast<float>(PentodeEL34::kGridCurrentScale),
        static_cast<float>(PentodeEL34::kGridThresholdVolts),
        static_cast<float>(PentodeEL34::kGridTransitionVolts) };

    /// @brief The constructor constants one PentodeStage takes for a named power tube, every
    /// voltage in real volts; the derivations are in the owning module's README.
    struct PowerTubePreset
    {
        /// @brief The Thevenin source resistance feeding the grid: the driver's output impedance
        /// plus the grid stopper.
        float source_resistance_ohms;

        /// @brief The grid-leak resistor from the grid to the bias supply.
        float grid_leak_resistance_ohms;

        /// @brief The grid's DC voltage against the cathode; the grid conducts |this| above rest.
        float cathode_bias_volts;

        /// @brief The inverting small-signal gain from grid to plate, gm times the reflected load.
        float gain;

        /// @brief The grid-current law the stage's node equation carries.
        GridConductionLaw grid_conduction;

        /// @brief The plate's swing toward cutoff and toward the bottom of its load line.
        PlateSwing plate_swing;
    };

    /// @brief EL34 as the JCM800 2203 runs it: the Philips EL34 datasheet's class B push-pull
    /// point at Vg1 -38 V, Raa 3.4 kOhm, 2 x 30 mA, 55 W (modules/nodes/jcm800/README.md, "The EL34
    /// pair"); source (82k || 43.056k) + 5.6k from the corrected inverter and jcm_pp_ot_nfb.
    inline constexpr PowerTubePreset kEl34{
        33831.9f, 220000.0f, -38.0f, 6.882361f, kPentodeEL34GridConduction, PlateSwing{ 25.5f, 280.2777f } };

    /// @brief Peavey 5150 lead, two 6L6GC per push-pull side: the RCA 6L6GC sheet's class AB1
    /// point (450 V, 400 V screen, Raa 5600 Ohm, 55 W) doubled, at the dossier's 2 x 30 mA idle
    /// and -55 V bias (modules/nodes/peavey5150/README.md, "The power tubes").
    inline constexpr PowerTubePreset kPeavey5150Power6L6GC{
        39000.0f, 220000.0f, -55.0f, 5.995340f, kPentodeEL34GridConduction, PlateSwing{ 42.0f, 350.4283f } };

    /// @brief Mesa Mark IIC+ Simul-Class inner pair, 6L6GC pentode: the RCA class AB1 point at its
    /// own 58 mA idle, the printed -45 V bias (modules/nodes/mesa_mark2c/README.md, "The output section").
    inline constexpr PowerTubePreset kMesaMark2cPower6L6GC{
        26642.4f, 220000.0f, -45.0f, 7.388139f, kPentodeEL34GridConduction, PlateSwing{ 81.2f, 311.2283f } };

    /// @brief Mesa Mark IIC+ Simul-Class outer pair, EL34 triode-strapped: Mesa's 25 W on the same
    /// primary, split evenly as a class A stage (modules/nodes/mesa_mark2c/README.md, "The output section").
    inline constexpr PowerTubePreset kMesaMark2cPowerEl34Triode{
        26642.4f, 220000.0f, -45.0f, 6.635772f, kPentodeEL34GridConduction, PlateSwing{ 132.2876f, 132.2876f } };

    /// @brief Diezel VH4, EL34 quad at 500 V: the Philips EL34 datasheet's 500 V push-pull point
    /// (Vg1 -36 V, Raa 4 kOhm, 70 W) at Diezel's 35 mA idle (modules/nodes/diezel_vh4/README.md, "The
    /// power tube is an EL34").
    inline constexpr PowerTubePreset kDiezelVh4PowerEl34{
        10000.0f, 220000.0f, -36.0f, 8.458480f, kPentodeEL34GridConduction, PlateSwing{ 35.0f, 339.1657f } };
}
