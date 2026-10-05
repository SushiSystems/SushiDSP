/****************************************************************/
/* Window.hpp                                                   */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

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
