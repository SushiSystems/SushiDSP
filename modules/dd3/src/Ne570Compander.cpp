/**************************************************************************/
/* Ne570Compander.cpp                                                     */
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
        // Eight sub-steps of this pole compose to the host-rate pole's coefficient exactly:
        // exp(-1/(8*fs*tau))^8 is exp(-1/(fs*tau)), so tau does not move with the sub-stepping.
        // Under a changing input the sub-stepped map differs from the host-rate one by about
        // 0.4 samples of envelope lag, negligible against tau's 326.4-sample time constant.
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
