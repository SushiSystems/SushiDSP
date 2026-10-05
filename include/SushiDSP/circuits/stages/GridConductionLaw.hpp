/****************************************************************/
/* GridConductionLaw.hpp                                        */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

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
}
