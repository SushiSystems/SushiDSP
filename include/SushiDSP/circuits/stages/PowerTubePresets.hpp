/**************************************************************************/
/* PowerTubePresets.hpp                                                   */
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
    /// @brief The constructor constants one PentodeStage takes for a named power tube.
    struct PowerTubePreset
    {
        float source_resistance_ohms;
        float grid_leak_resistance_ohms;
        float cathode_bias_volts;
        float gain;
        float saturation_current_amps;
        float thermal_voltage_volts;
        float plate_saturation_drive;
    };

    /// @brief EL34 as the JCM800 2203 runs it; the values Jcm800.cpp used before DA0, verbatim.
    inline constexpr PowerTubePreset kEl34{ 10000.0f, 220000.0f, -20.0f, 8.0f, 1e-9f, 0.02585f, 0.05f };
}
