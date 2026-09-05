/**************************************************************************/
/* ResonanceControl.cpp                                                  */
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

#include "SushiDSP/circuits/stages/ResonanceControl.hpp"

#include <algorithm>

namespace SushiDSP
{
    namespace
    {
        constexpr double kShelfQ = 0.7071067811865476;
    }

    ResonanceControl::ResonanceControl(double sample_rate, float corner_hz, float maximum_boost_db)
        : sample_rate_(sample_rate), corner_hz_(corner_hz), maximum_boost_db_(maximum_boost_db)
    {
        set_depth(0.0f);
    }

    void ResonanceControl::set_depth(float depth)
    {
        depth_ = std::clamp(depth, 0.0f, 1.0f);
        shelf_.set_low_shelf(sample_rate_, corner_hz_, boost_db(), kShelfQ);
    }

    float ResonanceControl::process_sample(float input) { return shelf_.process_sample(input); }
    void ResonanceControl::reset() { shelf_.reset(); }
    float ResonanceControl::boost_db() const { return depth_ * maximum_boost_db_; }
}
