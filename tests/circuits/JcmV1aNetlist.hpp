/**************************************************************************/
/* JcmV1aNetlist.hpp                                                      */
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
 * @file JcmV1aNetlist.hpp
 * @brief Defines the JCM800 2203 V1a stage as a DkNetlist for every test-side consumer.
 * @author Mustafa Garip
 */

#pragma once

#include <array>
#include <cstddef>

#include "SushiDSP/circuits/models/Triode12AX7.hpp"

namespace SushiDSP::test_circuits
{
    /// @brief Node numbering of docs/reference/spice/jcm_v1a, node for node; 0 is ground.
    ///
    /// Nodes 1 to 7 are the SPICE block. Nodes 8 and 9 exist only for the optional RC tail that
    /// build_jcm_v1a() appends when asked for the seven-state block.
    enum NodeIndex : std::size_t
    {
        kNodeSource = 1,
        kNodeGrid = 2,
        kNodePlate = 3,
        kNodeCathode = 4,
        kNodePotTop = 5,
        kNodeOut = 6,
        kNodeSupply = 7,
        kNodeTailMiddle = 8,
        kNodeTail = 9
    };

    /// @brief Component values of the JCM800 2203 V1a stage, transcribed from the SPICE block.
    inline constexpr double kGridLeakOhms = 1.0e6;
    inline constexpr double kGridStopperOhms = 68.0e3;
    inline constexpr double kPlateOhms = 100.0e3;
    inline constexpr double kCathodeOhms = 2.7e3;
    inline constexpr double kCathodeBypassFarads = 0.68e-6;
    inline constexpr double kCouplingFarads = 0.022e-6;
    inline constexpr double kBrightFarads = 470.0e-12;
    inline constexpr double kPreampPotOhms = 1.0e6;
    inline constexpr double kSupplyVolts = 360.0;

    /// @brief The 12AX7 interelectrode capacitances carried by the SPICE subcircuit itself.
    ///
    /// docs/reference/spice/models/12ax7_koren.sub declares these three inside the subckt, so
    /// every SPICE run of the block has them. Leaving them out puts the DK stage's input corner
    /// at a different frequency from the reference, which costs roughly 30 dB of null depth. Cgp
    /// is the Miller element and dominates: multiplied by the stage gain it, together with the
    /// grid stopper, sets the input rolloff near 22 kHz.
    inline constexpr double kGridCathodeFarads = 1.6e-12;
    inline constexpr double kGridPlateFarads = 1.7e-12;
    inline constexpr double kPlateCathodeFarads = 0.46e-12;

    /// @brief The optional RC tail: a second coupling stage into the next grid's leak resistor.
    ///
    /// Not part of the SPICE block. It exists so a caller can ask for the same circuit with one
    /// more reactive state, which is the shape the D6b performance budget is written against.
    inline constexpr double kTailSeriesOhms = 470.0e3;
    inline constexpr double kTailCouplingFarads = 0.022e-6;
    inline constexpr double kTailLoadOhms = 1.0e6;

    /// @brief Model glue handing DkCircuit the triode's two-branch current and Jacobian.
    ///
    /// Branch 0 is the grid branch (g to k) and branch 1 the plate branch (p to k), so the branch
    /// voltage vector is exactly the model's (Vgk, Vpk) and the current vector exactly its
    /// (Ig, Ip). The Jacobian is dense in its first column - dIp/dVgk is the stage's whole gain -
    /// which is what makes this the first exercise of DkCircuit's general `F * Jf` contraction.
    struct TriodeStagePort
    {
        SushiDSP::Triode12AX7 triode{};

        /// @brief Evaluates both branch currents and the 2x2 Jacobian at one port voltage.
        /// @param v Port voltages: v[0] = Vgk, v[1] = Vpk.
        /// @param i Receives the branch currents in amperes.
        /// @param j Receives the 2x2 Jacobian in siemens.
        void currents_and_jacobian(const std::array<double, 2>& v, std::array<double, 2>& i,
                                   std::array<std::array<double, 2>, 2>& j) const
        {
            triode.current_and_jacobian(v, i, j);
        }
    };

    /// @brief Builds the jcm_v1a netlist, node for node against the SPICE block's component table.
    /// @tparam Netlist A DkNetlist instantiation whose S matches the requested state count.
    /// @param netlist Receives the description; any previous contents are overwritten.
    /// @param with_rc_tail True to append the seventh-state RC tail and probe node 9.
    /// @return The preamp potentiometer's index, for DkCircuit::set_potentiometer.
    /// @see tests/circuits/README.md, "JcmV1aNetlist.hpp", for the two shapes of the block.
    template <typename Netlist>
    std::size_t build_jcm_v1a(Netlist& netlist, bool with_rc_tail = false)
    {
        netlist = Netlist{};
        netlist.node_count = with_rc_tail ? kNodeTail : kNodeSupply;

        netlist.resistor(kNodeSource, 0, kGridLeakOhms);
        netlist.resistor(kNodeSource, kNodeGrid, kGridStopperOhms);
        netlist.resistor(kNodeSupply, kNodePlate, kPlateOhms);
        netlist.resistor(kNodeCathode, 0, kCathodeOhms);

        netlist.capacitor(kNodeCathode, 0, kCathodeBypassFarads);
        netlist.capacitor(kNodePlate, kNodePotTop, kCouplingFarads);
        netlist.capacitor(kNodePotTop, kNodeOut, kBrightFarads);

        netlist.capacitor(kNodeGrid, kNodeCathode, kGridCathodeFarads);
        netlist.capacitor(kNodeGrid, kNodePlate, kGridPlateFarads);
        netlist.capacitor(kNodePlate, kNodeCathode, kPlateCathodeFarads);

        if (with_rc_tail)
        {
            netlist.resistor(kNodeOut, kNodeTailMiddle, kTailSeriesOhms);
            netlist.capacitor(kNodeTailMiddle, kNodeTail, kTailCouplingFarads);
            netlist.resistor(kNodeTail, 0, kTailLoadOhms);
        }

        netlist.nonlinear(kNodeGrid, kNodeCathode);
        netlist.nonlinear(kNodePlate, kNodeCathode);

        netlist.voltage_source(kNodeSource, 0);
        netlist.voltage_source(kNodeSupply, 0);

        netlist.output(with_rc_tail ? kNodeTail : kNodeOut, 0);

        return netlist.potentiometer(kNodePotTop, kNodeOut, 0, kPreampPotOhms);
    }

    /// @brief Converts the SPICE block's PREAMP fraction to a DkNetlist wiper position.
    ///
    /// The block writes the upper (pin to wiper) section as `1Meg*(1-PREAMP)`, while
    /// DkNetlist::potentiometer makes the a-to-wiper section `ohms * position`.
    /// @param preamp Normalised preamp-pot fraction; 0 is fully down.
    /// @return The equivalent wiper position.
    inline double preamp_to_position(double preamp)
    {
        return 1.0 - preamp;
    }
}
