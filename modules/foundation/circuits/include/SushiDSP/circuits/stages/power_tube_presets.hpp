/**************************************************************************/
/* power_tube_presets.hpp                                                 */
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
 * @file power_tube_presets.hpp
 * @brief Declares the constructor constants PentodeStage takes for a named power tube.
 * @author Mustafa Garip
 */

#pragma once

#include "SushiDSP/circuits/models/pentode_el34.hpp"
#include "SushiDSP/circuits/stages/grid_conduction_law.hpp"
#include "SushiDSP/circuits/stages/plate_swing.hpp"

namespace SushiDSP
{
    /// @brief The power tube's grid law, read from PentodeEL34 so the stage and the DK model agree.
    inline constexpr GridConductionLaw K_PENTODE_EL34_GRID_CONDUCTION{
        static_cast<float>(PentodeEL34::K_GRID_CURRENT_SCALE),
        static_cast<float>(PentodeEL34::K_GRID_THRESHOLD_VOLTS),
        static_cast<float>(PentodeEL34::K_GRID_TRANSITION_VOLTS)};

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

} // namespace SushiDSP
