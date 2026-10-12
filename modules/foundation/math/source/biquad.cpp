/**************************************************************************/
/* biquad.cpp                                                             */
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
 * @file biquad.cpp
 * @brief Implements the biquad filter.
 * @author Mustafa Garip
 */

// _USE_MATH_DEFINES precedes every include: it makes MSVC's <cmath> declare M_PI.
#define _USE_MATH_DEFINES

#include "SushiDSP/math/biquad.hpp"

#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace SushiDSP
{
    void Biquad::set_low_pass(double sample_rate, double cutoff_hz, double q)
    {
        const double omega = 2.0 * M_PI * cutoff_hz / sample_rate;
        const double sin_omega = std::sin(omega);
        const double cos_omega = std::cos(omega);
        const double alpha = sin_omega / (2.0 * q);

        const double b0 = (1.0 - cos_omega) / 2.0;
        const double b1 = 1.0 - cos_omega;
        const double b2 = (1.0 - cos_omega) / 2.0;
        const double a0 = 1.0 + alpha;
        const double a1 = -2.0 * cos_omega;
        const double a2 = 1.0 - alpha;

        set_coefficients(b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0);
    }

    void Biquad::set_one_pole_low_pass(double sample_rate, double cutoff_hz)
    {
        const double k = std::tan(M_PI * cutoff_hz / sample_rate);
        const double norm = 1.0 / (1.0 + k);

        set_coefficients(k * norm, k * norm, 0.0, (k - 1.0) * norm, 0.0);
    }

    void Biquad::set_first_order_high_pass(double sample_rate, double cutoff_hz)
    {
        const double pole = std::exp(-2.0 * M_PI * cutoff_hz / sample_rate);
        const double half_sum = (1.0 + pole) / 2.0;

        set_coefficients(half_sum, -half_sum, 0.0, -pole, 0.0);
    }

    void Biquad::set_high_pass(double sample_rate, double cutoff_hz, double q)
    {
        const double omega = 2.0 * M_PI * cutoff_hz / sample_rate;
        const double sin_omega = std::sin(omega);
        const double cos_omega = std::cos(omega);
        const double alpha = sin_omega / (2.0 * q);

        const double b0 = (1.0 + cos_omega) / 2.0;
        const double b1 = -(1.0 + cos_omega);
        const double b2 = (1.0 + cos_omega) / 2.0;
        const double a0 = 1.0 + alpha;
        const double a1 = -2.0 * cos_omega;
        const double a2 = 1.0 - alpha;

        set_coefficients(b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0);
    }

    void Biquad::set_low_shelf(double sample_rate, double cutoff_hz, double gain_db, double q)
    {
        const double a = std::pow(10.0, gain_db / 40.0);
        const double omega = 2.0 * M_PI * cutoff_hz / sample_rate;
        const double sin_omega = std::sin(omega);
        const double cos_omega = std::cos(omega);
        const double alpha = sin_omega / (2.0 * q);
        const double two_sqrt_a_alpha = 2.0 * std::sqrt(a) * alpha;

        const double b0 = a * ((a + 1.0) - (a - 1.0) * cos_omega + two_sqrt_a_alpha);
        const double b1 = 2.0 * a * ((a - 1.0) - (a + 1.0) * cos_omega);
        const double b2 = a * ((a + 1.0) - (a - 1.0) * cos_omega - two_sqrt_a_alpha);
        const double a0 = (a + 1.0) + (a - 1.0) * cos_omega + two_sqrt_a_alpha;
        const double a1 = -2.0 * ((a - 1.0) + (a + 1.0) * cos_omega);
        const double a2 = (a + 1.0) + (a - 1.0) * cos_omega - two_sqrt_a_alpha;

        set_coefficients(b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0);
    }

    void Biquad::set_high_shelf(double sample_rate, double cutoff_hz, double gain_db, double q)
    {
        const double a = std::pow(10.0, gain_db / 40.0);
        const double omega = 2.0 * M_PI * cutoff_hz / sample_rate;
        const double sin_omega = std::sin(omega);
        const double cos_omega = std::cos(omega);
        const double alpha = sin_omega / (2.0 * q);
        const double two_sqrt_a_alpha = 2.0 * std::sqrt(a) * alpha;

        const double b0 = a * ((a + 1.0) + (a - 1.0) * cos_omega + two_sqrt_a_alpha);
        const double b1 = -2.0 * a * ((a - 1.0) + (a + 1.0) * cos_omega);
        const double b2 = a * ((a + 1.0) + (a - 1.0) * cos_omega - two_sqrt_a_alpha);
        const double a0 = (a + 1.0) - (a - 1.0) * cos_omega + two_sqrt_a_alpha;
        const double a1 = 2.0 * ((a - 1.0) - (a + 1.0) * cos_omega);
        const double a2 = (a + 1.0) - (a - 1.0) * cos_omega - two_sqrt_a_alpha;

        set_coefficients(b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0);
    }

    void Biquad::set_peaking(double sample_rate, double center_hz, double gain_db, double q)
    {
        const double a = std::pow(10.0, gain_db / 40.0);
        const double omega = 2.0 * M_PI * center_hz / sample_rate;
        const double sin_omega = std::sin(omega);
        const double cos_omega = std::cos(omega);
        const double alpha = sin_omega / (2.0 * q);

        const double b0 = 1.0 + alpha * a;
        const double b1 = -2.0 * cos_omega;
        const double b2 = 1.0 - alpha * a;
        const double a0 = 1.0 + alpha / a;
        const double a1 = -2.0 * cos_omega;
        const double a2 = 1.0 - alpha / a;

        set_coefficients(b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0);
    }

    void Biquad::set_first_order_shelf(double sample_rate, double zero_hz, double pole_hz,
                                       double low_frequency_gain)
    {
        // t = tan(pi*f/fs) prewarps each corner separately, so both land exactly where intended.
        const double zero_tangent = std::tan(M_PI * zero_hz / sample_rate);
        const double pole_tangent = std::tan(M_PI * pole_hz / sample_rate);
        const double scale = low_frequency_gain * pole_tangent / zero_tangent;
        const double denominator = pole_tangent + 1.0;
        set_coefficients(scale * (zero_tangent + 1.0) / denominator,
                         scale * (zero_tangent - 1.0) / denominator, 0.0,
                         (pole_tangent - 1.0) / denominator, 0.0);
    }

    void Biquad::reset()
    {
        x1_ = x2_ = y1_ = y2_ = 0.0;
    }

    void Biquad::set_coefficients(double b0, double b1, double b2, double a1, double a2)
    {
        b0_ = b0;
        b1_ = b1;
        b2_ = b2;
        a1_ = a1;
        a2_ = a2;
    }
} // namespace SushiDSP
