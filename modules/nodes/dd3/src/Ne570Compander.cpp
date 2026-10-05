/****************************************************************/
/* Ne570Compander.cpp                                           */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file Ne570Compander.cpp
 * @brief NE570 compander half: an open-loop rectifier driving a square-law gain cell.
 * @author Mustafa Garip & Sushi Systems
 */

#include "SushiDSP/dd3/Ne570Compander.hpp"

#include <algorithm>
#include <cmath>

namespace SushiDSP::dd3
{
    namespace
    {
        constexpr double kFullWaveRectifierGain = 1.5707963267948966;   ///< pi/2, sine peak calibration.
    }

    Ne570Compander::Ne570Compander(CompanderHalf half)
        : half_(half)
    {
        prepare(sample_rate_);
    }

    void Ne570Compander::prepare(double sample_rate)
    {
        sample_rate_ = sample_rate;
        // The sub-stepped pole equals the host-rate one exactly; see modules/nodes/dd3/README.md.
        coefficient_ = 1.0 - std::exp(-1.0 / (sample_rate_ * kRectifierOversampling
                                              * kTimeConstantSeconds));
    }

    void Ne570Compander::reset()
    {
        envelope_ = 0.0;
        previous_ = 0.0;
    }

    void Ne570Compander::advance_envelope(double volts)
    {
        const double step = (volts - previous_) / static_cast<double>(kRectifierOversampling);
        double value = previous_;
        for (int sub = 0; sub < kRectifierOversampling; ++sub)
        {
            value += step;
            envelope_ += coefficient_ * (std::abs(value) * kFullWaveRectifierGain - envelope_);
        }
        previous_ = volts;
    }

    float Ne570Compander::process_sample(float volts)
    {
        const double input = static_cast<double>(volts);
        const double envelope_floor_volts = kReferenceVolts * kEnvelopeFloor;
        double output = 0.0;

        if (half_ == CompanderHalf::Compressor)
        {
            const double gain = kReferenceVolts / std::max(envelope_, envelope_floor_volts);
            output = input * gain;
            advance_envelope(output);
        }
        else
        {
            advance_envelope(input);
            const double gain = std::max(envelope_, envelope_floor_volts) / kReferenceVolts;
            output = input * gain;
        }

        return static_cast<float>(output);
    }

    double Ne570Compander::envelope_volts() const
    {
        return envelope_;
    }
}
