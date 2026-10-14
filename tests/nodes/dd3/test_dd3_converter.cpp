/**************************************************************************/
/* test_dd3_converter.cpp                                                 */
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
 * @file test_dd3_converter.cpp
 * @brief Tests for the DD-3 12-bit converter model.
 * @author Mustafa Garip & Sushi Systems
 */

// _USE_MATH_DEFINES precedes every include so that MSVC's <cmath> declares M_PI.
#define _USE_MATH_DEFINES

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>

#include <SushiDSP/nodes/dd3/dd3_converter.hpp>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

TEST(Dd3Converter, Dd3ConverterQuantisesOnItsExactCodeBoundaries)
{
    // step = 2 * 2.5 / 4096 = 5/4096 = 1.220703125e-3 V exactly  (DR1+DR2 plan, N8).
    using Converter = SushiDSP::dd3::Dd3Converter;
    EXPECT_DOUBLE_EQ(Converter::K_STEP_VOLTS, 1.220703125e-3);
    EXPECT_DOUBLE_EQ(Converter::quantise(0.0), 0.0);
    EXPECT_DOUBLE_EQ(Converter::quantise(0.49 * Converter::K_STEP_VOLTS), 0.0);
    EXPECT_DOUBLE_EQ(Converter::quantise(0.51 * Converter::K_STEP_VOLTS), Converter::K_STEP_VOLTS);
    EXPECT_DOUBLE_EQ(Converter::quantise(-0.51 * Converter::K_STEP_VOLTS),
                     -Converter::K_STEP_VOLTS);
    EXPECT_DOUBLE_EQ(Converter::quantise(1.0 * Converter::K_STEP_VOLTS), Converter::K_STEP_VOLTS);
    // The exact tie rounds away from zero, not to even.
    EXPECT_DOUBLE_EQ(Converter::quantise(0.5 * Converter::K_STEP_VOLTS), Converter::K_STEP_VOLTS);
    EXPECT_DOUBLE_EQ(Converter::quantise(-0.5 * Converter::K_STEP_VOLTS), -Converter::K_STEP_VOLTS);
}

TEST(Dd3Converter, Dd3ConverterSaturatesRatherThanWrapping)
{
    // Ruling 15. Code 2047 is 2047 * 1.220703125e-3 = 2.4987793 V; code -2048 is exactly -2.5 V.
    using Converter = SushiDSP::dd3::Dd3Converter;
    EXPECT_DOUBLE_EQ(Converter::quantise(10.0), 2047.0 * Converter::K_STEP_VOLTS);
    EXPECT_DOUBLE_EQ(Converter::quantise(1e6), 2047.0 * Converter::K_STEP_VOLTS);
    EXPECT_DOUBLE_EQ(Converter::quantise(-10.0), -2048.0 * Converter::K_STEP_VOLTS);
    EXPECT_NEAR(2047.0 * Converter::K_STEP_VOLTS, 2.4987793, 1e-6);
}

TEST(Dd3Converter, Dd3ConverterQuantisationNoiseMatchesTwelveBits)
{
    // An ideal 12-bit converter gives 74.008 dB of SNR. See tests/nodes/dd3/README.md.
    using Converter = SushiDSP::dd3::Dd3Converter;
    double signal_power = 0.0;
    double error_power = 0.0;
    for (std::size_t n = 0; n < 480000; ++n)
    {
        const double input = Converter::K_FULL_SCALE_VOLTS *
                             std::sin(2.0 * M_PI * 997.0 * static_cast<double>(n) / 48000.0);
        const double clamped = std::max(-Converter::K_FULL_SCALE_VOLTS,
                                        std::min(input, 2047.0 * Converter::K_STEP_VOLTS));
        const double output = Converter::quantise(clamped);
        signal_power += clamped * clamped;
        error_power += (output - clamped) * (output - clamped);
    }
    const double snr_db = 10.0 * std::log10(signal_power / error_power);
    std::printf("[ measure ] DD-3 converter SNR: %.3f dB (ideal 12-bit 74.008)\n", snr_db);
    EXPECT_NEAR(snr_db, 74.008, 1.0);
}

TEST(Dd3Converter, Dd3ConverterRingDelaysByExactlyNSamples)
{
    using SushiDSP::dd3::Dd3Mode;
    struct Case
    {
            Dd3Mode mode;
            std::uint32_t length;
    };
    const Case cases[3] = {
        {Dd3Mode::Short, 1024u}, {Dd3Mode::Middle, 4096u}, {Dd3Mode::Long, 16384u}};
    for (const Case& item : cases)
    {
        SushiDSP::dd3::Dd3Converter converter;
        converter.prepare();
        converter.set_mode(item.mode);
        EXPECT_EQ(converter.ring_length(), item.length);
        std::uint32_t impulse_at = 0;
        for (std::uint32_t n = 0; n < 3u * item.length; ++n)
        {
            const float out = converter.process_sample(n == 0 ? 1.0f : 0.0f);
            if (out != 0.0f && impulse_at == 0)
            {
                impulse_at = n;
            }
        }
        EXPECT_EQ(impulse_at, item.length) << item.length;
    }
}

TEST(Dd3Converter, Dd3ConverterHoldKeepsTheLoopAndInhibitsWrites)
{
    // Long and Hold share N, so the ring is not cleared. See tests/nodes/dd3/README.md.
    SushiDSP::dd3::Dd3Converter converter;
    converter.prepare();
    converter.set_mode(SushiDSP::dd3::Dd3Mode::Long);
    for (std::uint32_t n = 0; n < 16384u; ++n)
    {
        converter.process_sample(n == 100u ? 1.0f : 0.0f);
    }
    converter.set_mode(SushiDSP::dd3::Dd3Mode::Hold);
    // The stored impulse returns each lap; writes are dropped. See tests/nodes/dd3/README.md.
    for (std::uint32_t lap = 0; lap < 3u; ++lap)
    {
        for (std::uint32_t n = 0; n < 16384u; ++n)
        {
            const float out = converter.process_sample(0.5f);
            if (n == 100u)
            {
                EXPECT_NEAR(out, 1.0f, SushiDSP::dd3::Dd3Converter::K_STEP_VOLTS);
            }
            else
            {
                EXPECT_FLOAT_EQ(out, 0.0f) << lap << " " << n;
            }
        }
    }
    EXPECT_EQ(converter.ring_length(), 16384u);
}

TEST(Dd3Converter, Dd3ConverterModeChangeClearsTheRingWhenNChanges)
{
    // Spec: "MODE changes swap N and clear the ring" - which is exactly the S/M/L case.
    SushiDSP::dd3::Dd3Converter converter;
    converter.prepare();
    converter.set_mode(SushiDSP::dd3::Dd3Mode::Short);
    for (std::uint32_t n = 0; n < 512u; ++n)
    {
        converter.process_sample(0.9f);
    }
    converter.set_mode(SushiDSP::dd3::Dd3Mode::Middle);
    EXPECT_EQ(converter.ring_length(), 4096u);
    for (std::uint32_t n = 0; n < 4096u; ++n)
    {
        EXPECT_FLOAT_EQ(converter.process_sample(0.0f), 0.0f) << n;
    }
}
