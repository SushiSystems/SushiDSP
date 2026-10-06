/**************************************************************************/
/* Window.hpp                                                             */
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
 * @file Window.hpp
 * @brief Defines the Blackman-Harris analysis window.
 * @author Mustafa Garip
 */

#pragma once

#define _USE_MATH_DEFINES
#include <cmath>
#include <cstdint>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace SushiDSP::measure
{
    /// @brief Four-term Blackman-Harris window (sidelobes below -92 dB), generated in double
    /// and cast once so the analysis floor is set by the FFT, not by the window.
    /// @param size Window length.
    /// @return The window samples.
    inline std::vector<float> blackman_harris(std::uint32_t size)
    {
        std::vector<float> window(size);
        for (std::uint32_t n = 0; n < size; ++n)
        {
            const double x = 2.0 * M_PI * static_cast<double>(n) / static_cast<double>(size - 1);
            window[n] = static_cast<float>(0.35875 - 0.48829 * std::cos(x) + 0.14128 * std::cos(2.0 * x) - 0.01168 * std::cos(3.0 * x));
        }
        return window;
    }
}
