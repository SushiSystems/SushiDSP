/**************************************************************************/
/* LongTailedPair.cpp                                                     */
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

/**
 * @file LongTailedPair.cpp
 * @brief The pair's shared-cathode solve and the two triode halves it drives.
 * @author Mustafa Garip & Sushi Systems
 */

#include "SushiDSP/circuits/stages/LongTailedPair.hpp"

#include <algorithm>
#include <cassert>

namespace SushiDSP
{
    namespace
    {
        /// @brief Returns a half's common-cathode gain on its own load line, mu Ra / (Ra + ra).
        /// @param tube The shared tube properties.
        /// @param plate_load_ohms The half's plate resistor.
        float half_gain(const LongTailedPairTube& tube, float plate_load_ohms)
        {
            return tube.amplification_factor * plate_load_ohms
                / (plate_load_ohms + tube.plate_resistance_ohms);
        }

        /// @brief Returns how much of the summed grid voltage the shared cathode follows.
        ///
        /// Both halves draw gm (Vg - Vk) into the tail, and the tail carries their sum, so
        /// Vk = gm Rt (Vga + Vgb) / (1 + 2 gm Rt). A tail long against 1 / gm sends this to one
        /// half, which is the equal-and-opposite split the pair is used for.
        /// @param tube The shared tube properties.
        /// @param tail_ohms The whole resistance from the joined cathodes to the reference.
        float cathode_coupling_for(const LongTailedPairTube& tube, float tail_ohms)
        {
            const float transconductance = tube.amplification_factor / tube.plate_resistance_ohms;
            const float loop = transconductance * tail_ohms;
            return loop / (1.0f + 2.0f * loop);
        }
    }

    LongTailedPair::LongTailedPair(const LongTailedPairTube& tube, const LongTailedPairHalf& half_a,
                                   const LongTailedPairHalf& half_b, float tail_resistance_ohms,
                                   float cathode_to_tail_ohms)
        : cathode_coupling_(
              cathode_coupling_for(tube, cathode_to_tail_ohms + tail_resistance_ohms))
        , half_a_(half_a.source_resistance_ohms, half_a.grid_leak_resistance_ohms,
                  tube.cathode_bias_volts, half_gain(tube, half_a.plate_load_ohms),
                  tube.grid_conduction, half_a.plate_swing)
        , half_b_(half_b.source_resistance_ohms, half_b.grid_leak_resistance_ohms,
                  tube.cathode_bias_volts, half_gain(tube, half_b.plate_load_ohms),
                  tube.grid_conduction, half_b.plate_swing)
    {
        assert(tube.amplification_factor > 0.0f && tube.plate_resistance_ohms > 0.0f);
        assert(tube.cathode_bias_volts < 0.0f);
    }

    LongTailedPair::Plates LongTailedPair::process_sample(float grid_a, float grid_b)
    {
        const float cathode = cathode_coupling_ * (grid_a + grid_b);
        return Plates{ half_a_.process_sample(grid_a - cathode),
                       half_b_.process_sample(grid_b - cathode) };
    }

    float LongTailedPair::peak_grid_excursion_volts() const
    {
        return std::max(half_a_.peak_grid_excursion_volts(), half_b_.peak_grid_excursion_volts());
    }

    void LongTailedPair::reset()
    {
        half_a_.reset();
        half_b_.reset();
    }
}
