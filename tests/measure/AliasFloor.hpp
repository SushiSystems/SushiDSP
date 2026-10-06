/**************************************************************************/
/* AliasFloor.hpp                                                         */
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
 * @file AliasFloor.hpp
 * @brief Defines the alias-floor measurement of a node across 1 to 20 kHz test tones.
 * @author Mustafa Garip
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "SushiDSP/core/IAudioNode.hpp"
#include "measure/Render.hpp"
#include "measure/Spectrum.hpp"

namespace SushiDSP::measure
{
    /// @brief Returns the worst non-harmonic spectral line a node produces across 1..20 kHz tones.
    /// @param node The node under test, knobs already set.
    /// @param amplitude Peak stimulus in full-scale units; -12 dBFS by default.
    /// @return Worst alias level in dB relative to the fundamental (more negative is better).
    /// @see tests/measure/README.md, "AliasFloor.hpp", for the masking rule and the level caveat.
    inline double alias_floor(IAudioNode& node, double sample_rate, float amplitude = 0.251189f)
    {
        const std::size_t settle = static_cast<std::size_t>(sample_rate);
        const std::uint32_t low_bin = bin_of(20.0, sample_rate);
        const std::uint32_t high_bin = bin_of(std::min(20000.0, sample_rate / 2.0 - 1.0), sample_rate);
        double worst = -300.0;
        for (double tone = 1000.0; tone <= 20000.0 + 1e-9; tone += 1000.0)
        {
            const double f0 = bin_exact_frequency(tone, sample_rate, kFftSize);
            node.reset();
            const auto output = render(node, sine_stimulus(f0, sample_rate, settle + kFftSize, amplitude), sample_rate, 256);
            const auto spectrum = magnitude_spectrum_db(output, settle);
            const double fundamental = peak_near(spectrum, bin_of(f0, sample_rate));
            std::vector<bool> masked(spectrum.size(), false);
            auto mask = [&](std::uint32_t centre)
            {
                for (std::uint32_t bin = centre > 4 ? centre - 4 : 0; bin <= centre + 4 && bin < masked.size(); ++bin)
                {
                    masked[bin] = true;
                }
            };
            mask(0);
            for (double harmonic = f0; harmonic < sample_rate / 2.0; harmonic += f0)
            {
                mask(bin_of(harmonic, sample_rate));
            }
            for (std::uint32_t bin = low_bin; bin <= high_bin; ++bin)
            {
                if (!masked[bin])
                {
                    worst = std::max(worst, spectrum[bin] - fundamental);
                }
            }
        }
        return worst;
    }
}
