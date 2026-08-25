/**************************************************************************/
/* OutputTransformerStage.cpp                                             */
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

#include "SushiDSP/jcm800/OutputTransformerStage.hpp"

#include <cmath>

namespace SushiDSP
{
    OutputTransformerStage::OutputTransformerStage(
        double sample_rate, float low_rolloff_hz, float leakage_shelf_hz, float saturation_drive)
        : saturation_drive_(saturation_drive)
    {
        rolloff_.set_high_pass(sample_rate, low_rolloff_hz, 0.707);
        leakage_.set_low_pass(sample_rate, leakage_shelf_hz, 0.707);
    }

    float OutputTransformerStage::process_sample(float input)
    {
        float x = rolloff_.process_sample(input);
        x = leakage_.process_sample(x);
        return std::tanh(x * saturation_drive_) / saturation_drive_;
    }

    void OutputTransformerStage::reset()
    {
        rolloff_.reset();
        leakage_.reset();
    }
}
