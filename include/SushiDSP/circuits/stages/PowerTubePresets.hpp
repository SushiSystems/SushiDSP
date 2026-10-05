/****************************************************************/
/* PowerTubePresets.hpp                                         */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file PowerTubePresets.hpp
 * @brief Declares the constructor constants PentodeStage takes for a named power tube.
 * @author Mustafa Garip
 */

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

}
