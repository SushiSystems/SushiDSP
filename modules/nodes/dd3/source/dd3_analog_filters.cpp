/**************************************************************************/
/* dd3_analog_filters.cpp                                                 */
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
 * @file dd3_analog_filters.cpp
 * @brief Places the DD-3's emphasis shelves and Sallen-Key cascades at a sample rate.
 * @author Mustafa Garip & Sushi Systems
 */

// _USE_MATH_DEFINES must precede every include: it is what makes MSVC's <cmath> declare M_PI.
#define _USE_MATH_DEFINES

#include "SushiDSP/dd3/dd3_analog_filters.hpp"

#include <cmath>
#include <cstddef>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace SushiDSP::dd3
{
    double emitter_follower_damped_q(double unity_gain_q)
    {
        return 1.0 / (1.0 / unity_gain_q + 2.0 * (1.0 - K_EMITTER_FOLLOWER_GAIN) * unity_gain_q);
    }

    double prewarped_corner_hz(double corner_hz, double sample_rate)
    {
        const double edge_tangent = std::tan(M_PI * K_PASSBAND_EDGE_HZ / sample_rate);
        return sample_rate / M_PI * std::atan(corner_hz * edge_tangent / K_PASSBAND_EDGE_HZ);
    }

    void Dd3PreEmphasis::prepare(double sample_rate)
    {
        shelf_.set_first_order_shelf(sample_rate, K_EMPHASIS_ZERO_HZ, K_EMPHASIS_POLE_HZ,
                                     K_PRE_EMPHASIS_LOW_GAIN);
        shelf_.reset();
    }

    void Dd3PreEmphasis::reset()
    {
        shelf_.reset();
    }

    float Dd3PreEmphasis::process_sample(float input)
    {
        return shelf_.process_sample(input);
    }

    void Dd3DeEmphasis::prepare(double sample_rate)
    {
        shelf_.set_first_order_shelf(sample_rate, K_EMPHASIS_POLE_HZ, K_EMPHASIS_ZERO_HZ,
                                     K_DE_EMPHASIS_LOW_GAIN);
        shelf_.reset();
    }

    void Dd3DeEmphasis::reset()
    {
        shelf_.reset();
    }

    float Dd3DeEmphasis::process_sample(float input)
    {
        return shelf_.process_sample(input);
    }

    void Dd3AntiAliasFilter::prepare(double sample_rate)
    {
        for (std::size_t section = 0; section < K_SECTION_COUNT; ++section)
        {
            sections_[section].set_low_pass(
                sample_rate,
                prewarped_corner_hz(K_ANTI_ALIAS_SECTION_FREQUENCIES[section], sample_rate),
                emitter_follower_damped_q(K_ANTI_ALIAS_SECTION_Q[section]));
        }
        reset();
    }

    void Dd3AntiAliasFilter::reset()
    {
        for (std::size_t section = 0; section < K_SECTION_COUNT; ++section)
        {
            sections_[section].reset();
        }
    }

    float Dd3AntiAliasFilter::process_sample(float input)
    {
        float sample = input;
        for (std::size_t section = 0; section < K_SECTION_COUNT; ++section)
        {
            sample = sections_[section].process_sample(sample);
        }
        return sample;
    }

    void Dd3ReconstructionFilter::prepare(double sample_rate)
    {
        for (std::size_t section = 0; section < K_SECTION_COUNT; ++section)
        {
            sections_[section].set_low_pass(
                sample_rate,
                prewarped_corner_hz(K_RECONSTRUCTION_SECTION_FREQUENCIES[section], sample_rate),
                emitter_follower_damped_q(K_RECONSTRUCTION_SECTION_Q[section]));
        }
        reset();
    }

    void Dd3ReconstructionFilter::reset()
    {
        for (std::size_t section = 0; section < K_SECTION_COUNT; ++section)
        {
            sections_[section].reset();
        }
    }

    float Dd3ReconstructionFilter::process_sample(float input)
    {
        float sample = input;
        for (std::size_t section = 0; section < K_SECTION_COUNT; ++section)
        {
            sample = sections_[section].process_sample(sample);
        }
        return sample;
    }
} // namespace SushiDSP::dd3
