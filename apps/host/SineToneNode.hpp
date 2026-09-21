/**************************************************************************/
/* SineToneNode.hpp                                                       */
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

namespace SushiDSP
{
    /// @brief Diagnostic sine-wave generator for the standalone host's device sanity check.
    /// Not a product — no NodeDescriptor, not part of the public SushiDSP API.
    class SineToneNode : public IAudioNode
    {
    public:
        /// @brief Constructs a sine generator at the given frequency.
        /// @param frequency_hz The tone's frequency in Hz.
        explicit SineToneNode(float frequency_hz);

        void prepare(const PrepareInfo& info) override;

        void process(ProcessContext& context) override;

        PortLayout ports() const override;

        void reset() override;

        float get_parameter(const char*) const override { return 0.0f; }
        void set_parameter_for_control_thread(const char*, float) override {}

    private:
        float  frequency_hz_;
        double sample_rate_ = 48000.0;
        double phase_ = 0.0;
    };
}
