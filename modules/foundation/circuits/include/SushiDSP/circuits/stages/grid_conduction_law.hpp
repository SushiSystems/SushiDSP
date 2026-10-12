/**************************************************************************/
/* grid_conduction_law.hpp                                                */
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
 * @file grid_conduction_law.hpp
 * @brief Declares the three constants of the Norman grid-current law.
 * @author Mustafa Garip
 */

#pragma once

namespace SushiDSP
{
    /// @brief The Norman grid-current law Ig = scale * softplus((Vgk - threshold) / transition),
    /// the law the docs/reference/spice/models/*_koren.sub subcircuits evaluate.
    struct GridConductionLaw
    {
            /// @brief The current scale IG0, in amperes; scale / transition is the conducting slope.
            float scale_amps;

            /// @brief The knee's centre VGAMMA above the cathode, in volts.
            float threshold_volts;

            /// @brief The knee's width VTG, in volts.
            float transition_volts;
    };
} // namespace SushiDSP
