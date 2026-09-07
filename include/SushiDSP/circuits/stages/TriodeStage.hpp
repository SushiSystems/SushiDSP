/**************************************************************************/
/* TriodeStage.hpp                                                        */
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

#include "SushiDSP/circuits/models/Triode12AX7.hpp"

namespace SushiDSP
{
    /// @brief How far a stage's plate can swing from its idle point before it saturates.
    struct PlateSwing
    {
        /// @brief B+ minus the idle plate voltage; the cutoff side of the swing.
        float headroom_to_supply_volts;

        /// @brief Idle plate voltage minus the plate at Vgk = 0 on the load line; the
        /// conduction side of the swing.
        float headroom_to_bottom_volts;
    };

    /// @brief The Norman grid-current law Ig = scale * softplus((Vgk - threshold) / transition),
    /// the law docs/reference/spice/models/12ax7_koren.sub and Triode12AX7 evaluate.
    struct GridConductionLaw
    {
        /// @brief The current scale IG0, in amperes; scale / transition is the conducting slope.
        float scale_amps;

        /// @brief The knee's centre VGAMMA above the cathode, in volts.
        float threshold_volts;

        /// @brief The knee's width VTG, in volts.
        float transition_volts;
    };

    /// @brief The 12AX7's grid law, read from Triode12AX7 so the stage and the DK model agree.
    inline constexpr GridConductionLaw kTriode12AX7GridConduction{
        static_cast<float>(Triode12AX7::kGridCurrentScale),
        static_cast<float>(Triode12AX7::kGridThresholdVolts),
        static_cast<float>(Triode12AX7::kGridTransitionVolts) };

    /// @brief A common-cathode triode gain stage: the grid node solved by Newton-Raphson against
    /// a Norman grid-conduction law referenced to the cathode, an inverting linear plate gain,
    /// and a two-sided tanh plate ceiling from the stage's PlateSwing. Specified by
    /// docs/superpowers/specs/2026-09-07-triode-plate-swing-design.md section 3 and
    /// docs/superpowers/specs/2026-09-07-grid-knee-antialiasing-design.md section 3.
    class TriodeStage
    {
    public:
        /// @brief Builds a stage whose plate is bounded by its swing and whose grid conducts on
        /// the given law, referenced to the cathode.
        /// @param source_resistance_ohms Effective source resistance feeding the grid.
        /// @param grid_leak_resistance_ohms The grid-leak resistor to the cathode-bias voltage.
        /// @param cathode_bias_volts The stage's DC operating point, Vgk at rest.
        /// @param gain The common-cathode stage's inverting linear gain from grid to plate.
        /// @param grid_conduction The grid-current law's three constants.
        /// @param plate_swing The two headroom figures that bound the plate output.
        /// @pre cathode_bias_volts < 0, so the grid rests below the cathode.
        /// @pre gain > 0, and both plate_swing headrooms > 0.
        TriodeStage(
            float source_resistance_ohms, float grid_leak_resistance_ohms, float cathode_bias_volts,
            float gain, const GridConductionLaw& grid_conduction, const PlateSwing& plate_swing);

        /// @brief Processes one sample through the stage.
        /// @param input_voltage The stage's input signal voltage for this sample, referenced to
        /// the stage's own bias point; a zero input leaves the grid at the cathode-bias voltage.
        /// @return The inverted plate output voltage, bounded by the plate swing. Always finite.
        float process_sample(float input_voltage);

        /// @brief Returns the largest |Vg - Vk| the grid has reached since the last reset().
        float peak_grid_excursion_volts() const { return peak_grid_excursion_; }

        /// @brief Resets the Newton-Raphson warm start to the bias point and clears the peak.
        void reset();

    private:
        /// @brief Effective source resistance feeding the grid, in ohms.
        float source_resistance_;

        /// @brief The grid-leak resistance to the bias point, in ohms.
        float grid_leak_resistance_;

        /// @brief The grid's resting voltage relative to the cathode, in volts.
        float cathode_bias_;

        /// @brief The inverting linear gain from grid to plate.
        float gain_;

        /// @brief The grid-current law the node equation carries.
        GridConductionLaw grid_conduction_;

        /// @brief The two headrooms the plate tanh is bounded by.
        PlateSwing plate_swing_;

        /// @brief The grid voltage below which the plate tanh has saturated toward the supply.
        float cutoff_grid_limit_;

        /// @brief The grid voltage above which the plate tanh has saturated toward the bottom.
        float conduction_grid_limit_;

        /// @brief The previous sample's grid voltage, the solver's warm start.
        float previous_grid_voltage_;

        /// @brief The largest |Vg - Vk| reached since the last reset().
        float peak_grid_excursion_ = 0.0f;
    };
}
