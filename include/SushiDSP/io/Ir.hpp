/****************************************************************/
/* Ir.hpp                                                       */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

#pragma once

#include <vector>

namespace SushiDSP
{
    /// @brief A mono impulse response together with the sample rate it is valid at.
    struct Ir
    {
        std::vector<float> samples;
        double             sample_rate = 0.0;
    };
}
