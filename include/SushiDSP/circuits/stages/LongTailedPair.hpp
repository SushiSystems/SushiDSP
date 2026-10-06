/**************************************************************************/
/* LongTailedPair.hpp                                                     */
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
 * @file LongTailedPair.hpp
 * @brief A differential phase inverter: two triodes on one tail, two plates out.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include "SushiDSP/circuits/stages/GridConductionLaw.hpp"
#include "SushiDSP/circuits/stages/PlateSwing.hpp"
#include "SushiDSP/circuits/stages/TriodeStage.hpp"

namespace SushiDSP
{
    /// @brief The tube properties both halves of a pair share, at one operating point.
    struct LongTailedPairTube
    {
        float amplification_factor;        ///< mu, the local value at the operating point.
        float plate_resistance_ohms;       ///< r_a at the same point.
        float cathode_bias_volts;          ///< Vgk at rest, negative.
        GridConductionLaw grid_conduction; ///< The grid-current law both grids conduct on.
    };

    /// @brief One half's own plate load, headroom and grid drive.
    ///
    /// The two halves carry different plate loads in most amplifiers, which is how the pair's two
    /// outputs come out matched in amplitude despite the tail's asymmetry, and different grid leaks,
    /// because one grid is driven from the preamplifier and the other from the feedback leg.
    struct LongTailedPairHalf
    {
        float plate_load_ohms;           ///< The plate resistor this half works into.
        float source_resistance_ohms;    ///< Thevenin resistance feeding this half's grid.
        float grid_leak_resistance_ohms; ///< The leak pulling this grid toward the bias point.
        PlateSwing plate_swing;          ///< The two headrooms bounding this half's plate.
    };

    /// @brief A long-tailed pair phase inverter, solved as two triodes sharing a cathode node.
    ///
    /// Both grids are inputs: one carries the forward signal, the other whatever the amplifier
    /// feeds back. The shared cathode follows the sum of the two grids through the tail, which is
    /// what turns a common-mode grid move into no differential output and a differential move into
    /// two opposed plate swings. Each half is a TriodeStage driven by its own grid-to-cathode
    /// voltage, so grid conduction and the plate ceiling stay that class's business.
    class LongTailedPair
    {
    public:
        /// @brief The pair's two plate outputs for one sample.
        struct Plates
        {
            float a;  ///< The half driven by grid_a.
            float b;  ///< The half driven by grid_b.
        };

        /// @param tube What both halves are, at the operating point the plate swings belong to.
        /// @param half_a The half whose grid takes the forward signal.
        /// @param half_b The half whose grid takes the feedback.
        /// @param tail_resistance_ohms Resistance from the tail node to the pair's reference.
        /// @param cathode_to_tail_ohms Resistance between the joined cathodes and the tail node.
        /// @pre tube.plate_resistance_ohms > 0 and tube.amplification_factor > 0.
        /// @pre tube.cathode_bias_volts < 0, so both grids rest below the cathodes.
        LongTailedPair(const LongTailedPairTube& tube, const LongTailedPairHalf& half_a,
                       const LongTailedPairHalf& half_b, float tail_resistance_ohms,
                       float cathode_to_tail_ohms);

        /// @brief Processes one sample through both halves.
        /// @param grid_a This sample's voltage at the first grid, referenced to the bias point.
        /// @param grid_b This sample's voltage at the second grid, same reference.
        /// @return Both plate outputs, each bounded by its half's swing. Always finite.
        Plates process_sample(float grid_a, float grid_b);

        /// @brief Returns how much of the summed grid voltage the shared cathode follows.
        ///
        /// One half over this value is the factor the pair divides a single-ended input by, which
        /// a bench measurement of the drive at either plate checks.
        float cathode_coupling() const { return cathode_coupling_; }

        /// @brief Returns the larger |Vg - Vk| either half has reached since the last reset().
        float peak_grid_excursion_volts() const;

        /// @brief Resets both halves.
        void reset();

    private:
        /// @brief How much of grid_a + grid_b the shared cathode follows, gm Rt / (1 + 2 gm Rt).
        float cathode_coupling_;

        /// @brief The half driven by grid_a.
        TriodeStage half_a_;

        /// @brief The half driven by grid_b.
        TriodeStage half_b_;
    };
}
