/**************************************************************************/
/* ir.hpp                                                                 */
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
 * @file ir.hpp
 * @brief Declares a mono impulse response and the sample rate it is valid at.
 * @author Mustafa Garip
 */

#pragma once

#include <vector>

namespace SushiDSP
{
    /// @brief A mono impulse response together with the sample rate it is valid at.
    struct Ir
    {
            std::vector<float> samples;
            double sample_rate = 0.0;
    };
} // namespace SushiDSP
