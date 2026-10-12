/**************************************************************************/
/* ne570_compander.cpp                                                    */
/**************************************************************************/
/*                          This file is part of:                         */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                         https://sushisystems.io                        */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the PolyForm Noncommercial License 1.0.0 (the           */
/* "License"); you may not use this file except in compliance with the    */
/* License. You may obtain a copy of the License at                       */
/*                                                                        */
/*     https://polyformproject.org/licenses/noncommercial/1.0.0           */
/*                                                                        */
/* Noncommercial use is free. Commercial use requires a separate licence  */
/* from Sushi Systems; see COMMERCIAL.md. The software is provided        */
/* "as is", without warranty of any kind.                                 */
/**************************************************************************/

/**
 * @file ne570_compander.cpp
 * @brief NE570 compander half: an open-loop rectifier driving a square-law gain cell.
 * @author Mustafa Garip & Sushi Systems
 */

#include "SushiDSP/dd3/ne570_compander.hpp"

#include <algorithm>
#include <cmath>

namespace SushiDSP::dd3
{
    namespace
    {
        constexpr double K_FULL_WAVE_RECTIFIER_GAIN =
            1.5707963267948966; ///< pi/2, sine peak calibration.
    }

    Ne570Compander::Ne570Compander(CompanderHalf half) : half_(half)
    {
        prepare(sample_rate_);
    }

    void Ne570Compander::prepare(double sample_rate)
    {
        sample_rate_ = sample_rate;
        // The sub-stepped pole equals the host-rate one exactly; see modules/nodes/dd3/README.md.
        coefficient_ =
            1.0 -
            std::exp(-1.0 / (sample_rate_ * K_RECTIFIER_OVERSAMPLING * K_TIME_CONSTANT_SECONDS));
    }

    void Ne570Compander::reset()
    {
        envelope_ = 0.0;
        previous_ = 0.0;
    }

    void Ne570Compander::advance_envelope(double volts)
    {
        const double step = (volts - previous_) / static_cast<double>(K_RECTIFIER_OVERSAMPLING);
        double value = previous_;
        for (int sub = 0; sub < K_RECTIFIER_OVERSAMPLING; ++sub)
        {
            value += step;
            envelope_ += coefficient_ * (std::abs(value) * K_FULL_WAVE_RECTIFIER_GAIN - envelope_);
        }
        previous_ = volts;
    }

    float Ne570Compander::process_sample(float volts)
    {
        const double input = static_cast<double>(volts);
        const double envelope_floor_volts = K_REFERENCE_VOLTS * K_ENVELOPE_FLOOR;
        double output = 0.0;

        if (half_ == CompanderHalf::Compressor)
        {
            const double gain = K_REFERENCE_VOLTS / std::max(envelope_, envelope_floor_volts);
            output = input * gain;
            advance_envelope(output);
        }
        else
        {
            advance_envelope(input);
            const double gain = std::max(envelope_, envelope_floor_volts) / K_REFERENCE_VOLTS;
            output = input * gain;
        }

        return static_cast<float>(output);
    }

    double Ne570Compander::envelope_volts() const
    {
        return envelope_;
    }
} // namespace SushiDSP::dd3
