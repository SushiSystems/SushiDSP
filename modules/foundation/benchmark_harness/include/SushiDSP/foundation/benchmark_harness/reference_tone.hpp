/**************************************************************************/
/* reference_tone.hpp                                                     */
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
 * @file reference_tone.hpp
 * @brief Declares the 220 Hz tone every benchmark case feeds its subject.
 * @author Mustafa Garip
 */

#pragma once

#include <cmath>
#include <cstdint>
#include <vector>

#include <SushiDSP/foundation/benchmark_harness/benchmark_harness.hpp>

namespace SushiDSP::bench
{
    /// @brief Holds the reference tone's frequency, the one the regression snapshots use.
    inline constexpr double K_REFERENCE_TONE_HZ = 220.0;

    /// @brief Holds the reference tone's peak level, in full scale.
    inline constexpr float K_REFERENCE_TONE_LEVEL = 0.25f;

    /// @brief Returns the reference tone at K_SAMPLE_RATE, starting at phase zero.
    /// @param frames Samples to generate.
    inline std::vector<float> reference_tone(std::uint32_t frames)
    {
        constexpr double K_TWO_PI = 6.283185307179586476925;
        std::vector<float> tone(frames);
        for (std::uint32_t i = 0; i < frames; ++i)
        {
            tone[i] = K_REFERENCE_TONE_LEVEL *
                      static_cast<float>(std::sin(K_TWO_PI * K_REFERENCE_TONE_HZ *
                                                  static_cast<double>(i) / K_SAMPLE_RATE));
        }
        return tone;
    }
} // namespace SushiDSP::bench
