/**************************************************************************/
/* TriodeStage.hpp                                                        */
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
 * @file TriodeStage.hpp
 * @brief Declares a common-cathode triode gain stage.
 * @author Mustafa Garip
 */

#pragma once

#include "SushiDSP/circuits/models/Triode12AX7.hpp"
#include "SushiDSP/circuits/stages/AntiderivativeAntialiaser.hpp"
#include "SushiDSP/circuits/stages/GridConductionLaw.hpp"
#include "SushiDSP/circuits/stages/PlateSwing.hpp"

namespace SushiDSP
{
    /// @brief The 12AX7's grid law, read from Triode12AX7 so the stage and the DK model agree.
    inline constexpr GridConductionLaw kTriode12AX7GridConduction{
        static_cast<float>(Triode12AX7::kGridCurrentScale),
        static_cast<float>(Triode12AX7::kGridThresholdVolts),
        static_cast<float>(Triode12AX7::kGridTransitionVolts) };

    /// @brief A common-cathode triode gain stage: the grid node solved by Newton-Raphson against
    /// a Norman grid-conduction law referenced to the cathode, an inverting linear plate gain,
    /// and a two-sided tanh plate ceiling from the stage's PlateSwing, the composite transfer
    /// antialiased by an AntiderivativeAntialiaser. Specified by
    /// docs/archive/superpowers/specs/2026-09-07-triode-plate-swing-design.md section 3 and
    /// docs/archive/superpowers/specs/2026-09-07-grid-knee-antialiasing-design.md sections 3 and 15.
    class TriodeStage
    {
    public:
        /// @brief Builds a stage whose plate is bounded by its swing and whose grid conducts on
        /// the given law, referenced to the cathode.
        /// @param source_resistance_ohms Effective source resistance feeding the grid.
        /// @param grid_leak_resistance_ohms The grid-leak resistor to the cathode-bias voltage.
        /// @param cathode_bias_volts The stage's DC operating point, Vgk at rest.
        /// @param gain The common-cathode stage's inverting linear gain from grid to plate.
        /// @pre cathode_bias_volts < 0, so the grid rests below the cathode.
        /// @pre gain > 0, and both plate_swing headrooms > 0.
        TriodeStage(
            float source_resistance_ohms, float grid_leak_resistance_ohms, float cathode_bias_volts,
            float gain, const GridConductionLaw& grid_conduction, const PlateSwing& plate_swing);

        /// @brief Processes one sample through the stage.
        /// @param input_voltage The stage's input signal voltage for this sample, referenced to
        /// the stage's own bias point; a zero input leaves the grid at the cathode-bias voltage.
        /// @return The inverted plate output voltage, bounded by the plate swing and antialiased
        /// over the interval from the previous sample. Always finite.
        float process_sample(float input_voltage);

        /// @brief Returns the largest |Vg - Vk| the grid has reached since the last reset().
        float peak_grid_excursion_volts() const { return peak_grid_excursion_; }

        /// @brief Resets the Newton-Raphson warm start to the bias point, clears the peak and
        /// drops the antialiaser's previous sample.
        void reset();

    private:
        /// @brief Reports whether a warm start sitting on one of the grid bounds is already the
        /// solve's result, which the residual's sign there decides without the exponential.
        /// @param grid_voltage The warm start, the previous sample's root.
        /// @param source_voltage The cathode bias plus this sample's input.
        bool root_stays_on_bound(float grid_voltage, float source_voltage) const;

        /// @brief Effective source resistance feeding the grid, in ohms.
        float source_resistance_;

        /// @brief The grid-leak resistance to the bias point, in ohms.
        float grid_leak_resistance_;

        /// @brief The grid's resting voltage relative to the cathode, in volts.
        float cathode_bias_;

        /// @brief The grid-current law the node equation carries.
        GridConductionLaw grid_conduction_;

        /// @brief The grid voltage below which the plate tanh has saturated toward the supply.
        float cutoff_grid_limit_;

        /// @brief The grid voltage above which the plate tanh has saturated toward the bottom.
        float conduction_grid_limit_;

        /// @brief The previous sample's grid voltage, the solver's warm start.
        float previous_grid_voltage_;

        /// @brief The antialiaser of the composite grid-and-plate transfer.
        AntiderivativeAntialiaser antialiaser_;

        /// @brief The largest |Vg - Vk| reached since the last reset().
        float peak_grid_excursion_ = 0.0f;
    };
}
