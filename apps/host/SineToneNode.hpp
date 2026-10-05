/****************************************************************/
/* SineToneNode.hpp                                             */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file SineToneNode.hpp
 * @brief Declares the diagnostic sine generator of the standalone host.
 * @author Mustafa Garip
 */

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
