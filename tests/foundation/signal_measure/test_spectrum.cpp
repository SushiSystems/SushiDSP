/**************************************************************************/
/* test_spectrum.cpp                                                      */
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
 * @file test_spectrum.cpp
 * @brief Tests the bin helpers and the magnitude spectrum of a pure tone.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/signal_measure/render.hpp>
#include <SushiDSP/foundation/signal_measure/spectrum.hpp>

#include <cmath>
#include <cstddef>
#include <vector>

TEST(Spectrum, BinOfMapsAFrequencyToItsFftBin)
{
    EXPECT_EQ(SushiDSP::measure::bin_of(1000.0, 48000.0), 1365u);
    EXPECT_EQ(SushiDSP::measure::bin_of(0.0, 48000.0), 0u);
}

TEST(Spectrum, BinExactFrequencyIsWithinHalfABinOfTheRequest)
{
    const double bin_width = 48000.0 / static_cast<double>(SushiDSP::measure::K_FFT_SIZE);
    const double exact = SushiDSP::measure::bin_exact_frequency(1000.0, 48000.0, SushiDSP::measure::K_FFT_SIZE);
    EXPECT_LE(std::fabs(exact - 1000.0), 0.5 * bin_width + 1e-9);
}

TEST(Spectrum, PureToneSpectrumPeaksAtItsBin)
{
    const std::vector<float> tone = SushiDSP::measure::sine_stimulus(1000.0, 48000.0,
                                                                     SushiDSP::measure::K_FFT_SIZE, 0.5f);
    const std::vector<double> spectrum = SushiDSP::measure::magnitude_spectrum_db(tone, 0);
    ASSERT_EQ(spectrum.size(), SushiDSP::measure::K_FFT_SIZE / 2 + 1);
    std::size_t peak = 0;
    for (std::size_t i = 1; i < spectrum.size(); ++i)
    {
        if (spectrum[i] > spectrum[peak])
        {
            peak = i;
        }
    }
    EXPECT_LE(std::abs(static_cast<long>(peak) - static_cast<long>(SushiDSP::measure::bin_of(1000.0, 48000.0))), 1L);
}
