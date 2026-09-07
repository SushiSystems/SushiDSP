/**************************************************************************/
/* GridConductionLaw.hpp                                                  */
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
