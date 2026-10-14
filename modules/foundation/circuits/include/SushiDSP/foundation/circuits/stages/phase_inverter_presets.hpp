/**************************************************************************/
/* phase_inverter_presets.hpp                                             */
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
 * @file phase_inverter_presets.hpp
 * @brief A long-tailed pair reduced to one triode, and the operating points the corpus carries.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <SushiDSP/foundation/circuits/stages/plate_swing.hpp>

namespace SushiDSP
{
    /// @brief Describes a long-tailed pair modelled as a single common-cathode triode: where its
    /// grid rests, what one triode's load line gives, what the shared cathode takes back, and the
    /// swing that bounds the plate.
    struct SingleTriodeInverter
    {
            /// @brief Holds the grid's resting voltage relative to the cathode, in volts.
            float cathode_bias_volts;

            /// @brief Holds one triode's full load-line gain, mu * Ra / (ra + Ra).
            /// @pre The pair's single-ended split belongs in input_degeneration, so halving this
            /// figure would count that split twice.
            float gain;

            /// @brief Holds the divisor the input takes because the shared cathode follows the driven
            /// grid, 1 + R_tail / (R_tail + (ra + Ra) / (mu + 1)).
            float input_degeneration;

            /// @brief Holds the two headrooms that bound the plate.
            PlateSwing plate_swing;
    };

    /// @brief A 12AX7 pair on an 82 kOhm plate over a 10.47 kOhm tail at 360 V, both grid leaks
    /// returned to the 470 Ohm and 10 kOhm junction: gm 2.193524 mA/V, ra 43055.615 Ohm, mu
    /// 94.443529, the cathodes at 29.984558 V, the plate at 242.581959 V idle and 123.049256 V of
    /// plate-to-cathode voltage at Vgk = 0 (docs/reference/spice/jcm_ltp).
    inline constexpr SingleTriodeInverter K_TRIODE12_AX7_INVERTER82K_PLATE{
        -1.346012f, 61.927402f, 1.888775f, PlateSwing{117.418041f, 89.548144f}};
} // namespace SushiDSP
