/**************************************************************************/
/* TubeScreamerCircuits.hpp                                               */
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

#include <array>
#include <cstddef>

#include "SushiDSP/circuits/models/SiliconDiode.hpp"
#include "SushiDSP/math/dk/DkCircuit.hpp"
#include "SushiDSP/math/dk/DkNetlist.hpp"

/// @file
/// @brief The TS808's two DK blocks: component values, node numbering, the nonlinear port glue
/// and the knob-to-wiper mappings, described once.
///
/// Both netlists follow docs/reference/spice/ts_clip/README.md and ts_tone/README.md node for
/// node, because the committed CSVs from those schematics are what the block tests null
/// against. A value that drifts from its README makes those nulls meaningless.
namespace SushiDSP::tube_screamer_circuits
{
    /// @brief Node numbering of docs/reference/spice/ts_clip, node for node; 0 is ground.
    ///
    /// kClipNodeFeedbackMiddle is the one node the reference does not have: it splits Rf into its
    /// fixed 51 kOhm part and its 500 kOhm rheostat part, which is how a three-terminal
    /// DkNetlist::potentiometer expresses a two-terminal variable resistor (D6c plan, ruling 2).
    enum ClipNodeIndex : std::size_t
    {
        kClipNodeSource = 1,
        kClipNodeInput = 2,
        kClipNodeOutput = 3,
        kClipNodeInverting = 4,
        kClipNodeGroundLeg = 5,
        kClipNodeFeedbackMiddle = 6
    };

    inline constexpr double kInputFarads = 1.0e-6;            ///< Cin (ts_clip README).
    inline constexpr double kInputLeakOhms = 510.0e3;         ///< Rin (ts_clip README).
    inline constexpr double kFeedbackFixedOhms = 51.0e3;      ///< Rf's fixed part (ts_clip README).
    inline constexpr double kDrivePotOhms = 500.0e3;          ///< Rf's Drive part (ts_clip README).
    inline constexpr double kFeedbackFarads = 51.0e-12;       ///< Cf (ts_clip README).
    inline constexpr double kGroundLegOhms = 4700.0;          ///< Rg (ts_clip README).
    inline constexpr double kGroundLegFarads = 47.0e-9;       ///< Cg (ts_clip README).

    /// @brief One diode's zero-bias junction capacitance, from d1n914.sub's Cjo.
    ///
    /// The SPICE model carries it, so every LTspice run of ts_clip has 4 pF across each diode and
    /// 8 pF in parallel with Cf. With Cf alone the DK block is 1.15 dB off the reference at
    /// 20 kHz; with the two junctions folded in it is 0.004 dB off across the whole sweep.
    /// SPICE varies its junction capacitance with the diode voltage and this one does not, so at
    /// the clipping excursion the pair is 8.9 pF rather than 8.0 and the feedback pole sits
    /// 1.6 % low (D6c plan, ruling 3).
    inline constexpr double kDiodeJunctionFarads = 4.0e-12;

    /// @brief Node numbering of docs/reference/spice/ts_tone, node for node; 0 is ground.
    enum ToneNodeIndex : std::size_t
    {
        kToneNodeSource = 1,
        kToneNodeFilter = 2,
        kToneNodeWiper = 3,
        kToneNodeLegMiddle = 4,
        kToneNodeInverting = 5,
        kToneNodeStageOutput = 6,
        kToneNodeOutputCap = 7,
        kToneNodeLevelTop = 8,
        kToneNodeOutput = 9
    };

    inline constexpr double kInputSeriesOhms = 1.0e3;         ///< R7 (ts_tone README).
    inline constexpr double kInputShuntFarads = 220.0e-9;     ///< C5 (ts_tone README).
    inline constexpr double kBiasReturnOhms = 10.0e3;         ///< R9 (ts_tone README).
    inline constexpr double kTonePotOhms = 20.0e3;            ///< Tone pot (ts_tone README).
    inline constexpr double kToneLegFarads = 220.0e-9;        ///< C6 (ts_tone README).
    inline constexpr double kToneLegOhms = 220.0;             ///< R8 (ts_tone README).
    inline constexpr double kFeedbackOhms = 1.0e3;            ///< R11 (ts_tone README).
    inline constexpr double kOutputFarads = 1.0e-6;           ///< C7 (ts_tone README).
    inline constexpr double kOutputSeriesOhms = 1.0e3;        ///< R12 (ts_tone README).
    inline constexpr double kLevelPotOhms = 100.0e3;          ///< Level pot (ts_tone README).

    /// @brief Wiper position of the level pot, held where the reference holds it.
    ///
    /// ts_tone.asc writes the upper section as 100k*(1 - LEVEL) while DkNetlist::potentiometer
    /// makes it ohms * position, so position is 1 - LEVEL and the reference's LEVEL = 1 is
    /// position 0. The shipping Level control stays the calibrated dB output gain D5b defined and
    /// does not drive this pot; the pot is in the netlist so the block is the reference node for
    /// node, and so a later phase can expose it without re-deriving the circuit.
    inline constexpr double kLevelPotPosition = 0.0;

    using ClipNetlist = dk::DkNetlist<3, 2, 1, 1>;
    using ClipCircuit = dk::DkCircuit<3, 2, 1, 1>;
    using ToneNetlist = dk::DkNetlist<3, 0, 1, 1>;
    using ToneCircuit = dk::DkCircuit<3, 0, 1, 1>;

    /// @brief The clip stage's nonlinear port: an antiparallel 1N914 pair across the feedback.
    ///
    /// Branch 0 is D1 (anode at the op-amp output, cathode at the summing junction) and branch 1
    /// is D2, wired the other way, so v[1] is always -v[0] and the two branches are independent
    /// one-ports. The Jacobian is therefore diagonal, and DkCircuit zeroes the buffer before every
    /// call, so the off-diagonal entries are left alone. The pair is two branches rather than one
    /// antiparallel branch so an asymmetric pair stays a value change (spec D6c).
    struct DiodePairPort
    {
        SiliconDiode diode{};

        /// @brief Evaluates both diode currents and the diagonal Jacobian at one port voltage.
        /// @param v Branch voltages in volts: v[0] across D1, v[1] across D2.
        /// @param i Receives the branch currents in amperes.
        /// @param j Receives the 2x2 Jacobian in siemens; only the diagonal is written.
        void currents_and_jacobian(const std::array<double, 2>& v, std::array<double, 2>& i,
                                   std::array<std::array<double, 2>, 2>& j) const
        {
            diode.current_and_jacobian(v[0], i[0], j[0][0]);
            diode.current_and_jacobian(v[1], i[1], j[1][1]);
        }
    };

    /// @brief The empty nonlinear port a purely linear DK block is stepped with.
    struct LinearPort
    {
        /// @brief Does nothing; a block with no nonlinear branches has no currents to evaluate.
        void currents_and_jacobian(const std::array<double, 0>&, std::array<double, 0>&,
                                   std::array<std::array<double, 0>, 0>&) const {}
    };

    /// @brief Builds the ts_clip netlist, node for node against the block's component table.
    ///
    /// The drive pot is wired as the rheostat the pedal wires it as, its wiper tied to the
    /// summing junction, so the dead wiper-to-b section stamps nothing: both its terminals are
    /// the same node (D6c plan, ruling 2).
    /// @param netlist Receives the description; any previous contents are overwritten.
    /// @return The drive rheostat's index, for DkCircuit::set_potentiometer.
    inline std::size_t build_clip_block(ClipNetlist& netlist)
    {
        netlist = ClipNetlist{};
        netlist.node_count = kClipNodeFeedbackMiddle;

        netlist.voltage_source(kClipNodeSource, 0);
        netlist.capacitor(kClipNodeSource, kClipNodeInput, kInputFarads);
        netlist.resistor(kClipNodeInput, 0, kInputLeakOhms);

        netlist.resistor(kClipNodeOutput, kClipNodeFeedbackMiddle, kFeedbackFixedOhms);
        netlist.capacitor(kClipNodeOutput, kClipNodeInverting,
                          kFeedbackFarads + 2.0 * kDiodeJunctionFarads);
        netlist.nonlinear(kClipNodeOutput, kClipNodeInverting);
        netlist.nonlinear(kClipNodeInverting, kClipNodeOutput);

        netlist.resistor(kClipNodeInverting, kClipNodeGroundLeg, kGroundLegOhms);
        netlist.capacitor(kClipNodeGroundLeg, 0, kGroundLegFarads);

        netlist.nullor(kClipNodeInput, kClipNodeInverting, kClipNodeOutput, 0);
        netlist.output(kClipNodeOutput, 0);

        return netlist.potentiometer(kClipNodeFeedbackMiddle, kClipNodeInverting,
                                     kClipNodeInverting, kDrivePotOhms);
    }

    /// @brief The two wiper indices build_tone_block() hands back.
    struct TonePotentiometers
    {
        std::size_t tone = 0;   ///< The 20 kOhm tone pot.
        std::size_t level = 0;  ///< The 100 kOhm level pot, held at kLevelPotPosition.
    };

    /// @brief Builds the ts_tone netlist, node for node against the block's component table.
    ///
    /// The op-amp is non-inverting: the nullor senses the filter node against the inverting
    /// input and kFeedbackOhms closes the loop, so the tone pot's inverting-side section and
    /// the 220 nF / 220 Ohm leg together are the stage's gain leg. Wiring it as a virtual-earth
    /// integrator instead is the error the 2026-09-06 correction removed; ts_tone/README.md
    /// records it against the schematic.
    /// @param netlist Receives the description; any previous contents are overwritten.
    /// @return Both wiper indices, for DkCircuit::set_potentiometer.
    inline TonePotentiometers build_tone_block(ToneNetlist& netlist)
    {
        netlist = ToneNetlist{};
        netlist.node_count = kToneNodeOutput;

        netlist.voltage_source(kToneNodeSource, 0);
        netlist.resistor(kToneNodeSource, kToneNodeFilter, kInputSeriesOhms);
        netlist.capacitor(kToneNodeFilter, 0, kInputShuntFarads);
        netlist.resistor(kToneNodeFilter, 0, kBiasReturnOhms);

        TonePotentiometers pots;
        pots.tone = netlist.potentiometer(kToneNodeFilter, kToneNodeWiper, kToneNodeInverting,
                                          kTonePotOhms);
        netlist.capacitor(kToneNodeWiper, kToneNodeLegMiddle, kToneLegFarads);
        netlist.resistor(kToneNodeLegMiddle, 0, kToneLegOhms);

        netlist.resistor(kToneNodeInverting, kToneNodeStageOutput, kFeedbackOhms);
        netlist.nullor(kToneNodeFilter, kToneNodeInverting, kToneNodeStageOutput, 0);

        netlist.capacitor(kToneNodeStageOutput, kToneNodeOutputCap, kOutputFarads);
        netlist.resistor(kToneNodeOutputCap, kToneNodeLevelTop, kOutputSeriesOhms);
        pots.level = netlist.potentiometer(kToneNodeLevelTop, kToneNodeOutput, 0, kLevelPotOhms);
        netlist.output(kToneNodeOutput, 0);

        return pots;
    }

    /// @brief Converts the Drive knob to the rheostat's wiper position.
    ///
    /// ts_clip.asc writes Rf as 51k + DRIVE*500k and the rheostat's live section is
    /// ohms * position, so the mapping is the identity and the reference's DRIVE steps compare
    /// directly against knob values. No audio taper this phase (D6c plan, ruling 9).
    /// @param drive Normalised Drive knob, 0 to 1.
    /// @return The wiper position.
    inline double drive_to_position(double drive) { return drive; }

    /// @brief Converts the Tone knob to the tone pot's wiper position.
    ///
    /// ts_tone.asc measures TONE from the op-amp's + input, where the wiper leg only cuts
    /// treble, towards the - input, where it becomes the gain leg and boosts it; so TONE = 1 is
    /// bright and the mapping is the identity. Both end stops are ordinary settings on the
    /// corrected network - neither mutes and neither leaves a bare integrator - so the travel is
    /// the whole track. No audio taper this phase (D6c plan, ruling 9).
    /// @param tone Normalised Tone knob, 0 (dark) to 1 (bright).
    /// @return The wiper position.
    inline double tone_to_position(double tone) { return tone; }
}
