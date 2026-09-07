/**************************************************************************/
/* PanNode.hpp                                                            */
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

#include "SushiDSP/core/IAudioNode.hpp"
#include "SushiDSP/core/NodeDescriptor.hpp"
#include "SushiDSP/math/SmoothedParameter.hpp"

namespace SushiDSP
{
    /// @brief Turns one channel into two on a constant-power pan law: with
    /// theta = (pan + 1) * pi / 4, the left gain is cos(theta) and the right is sin(theta), so
    /// the centre sits at -3.01 dB on both channels and the sum of the squared gains is 1 at
    /// every position. The knob is smoothed; the two gains are recomputed only when it moved.
    class PanNode : public IAudioNode
    {
    public:
        PanNode();

        void prepare(const PrepareInfo& info) override;
        void process(ProcessContext& context) override;
        PortLayout ports() const override;
        void reset() override;
        float get_parameter(const char* id) const override;
        void set_parameter(const char* id, float value) override;
        void set_parameter(std::uint16_t index, float value) override;

        /// @brief This pan node's registered type id.
        /// @return "sushidsp.pan".
        const char* type_id() const override;

    private:
        /// @brief Recomputes gain_left_/gain_right_ from a moved pan value and updates the
        /// applied marker.
        /// @param pan The pan value the gains should now match.
        void apply_pan(float pan);

        SmoothedParameter pan_{ 0.0f };
        float applied_pan_ = -2.0f;   // outside [-1, 1]: forces the first block to compute
        float gain_left_ = 0.0f;
        float gain_right_ = 0.0f;
    };

    /// @brief The pan node's product registration entry point.
    /// @return The static NodeDescriptor for "sushidsp.pan".
    const NodeDescriptor& pan_descriptor();
}
