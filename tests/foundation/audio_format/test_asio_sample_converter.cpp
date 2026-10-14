/**************************************************************************/
/* test_asio_sample_converter.cpp                                         */
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
 * @file test_asio_sample_converter.cpp
 * @brief Tests the conversion between ASIO sample formats and float.
 * @author Mustafa Garip
 */

#include <cstdint>
#include <cstring>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/audio_format/asio_sample_converter.hpp>

TEST(AsioSampleConverter, AsioSampleConverterInt32RoundTripsKnownValues)
{
    // Int32LSB: full-scale is +/-2147483648.
    const std::int32_t half_scale_positive = 1073741824; // 0.5 * INT32_MAX+1
    std::uint8_t bytes[4];
    std::memcpy(bytes, &half_scale_positive, 4);

    const float decoded =
        SushiDSP::asio_sample_to_float(SushiDSP::AsioSampleFormat::Int32LSB, bytes);
    EXPECT_NEAR(decoded, 0.5f, 1e-6f);

    std::uint8_t re_encoded[4];
    SushiDSP::float_to_asio_sample(SushiDSP::AsioSampleFormat::Int32LSB, 0.5f, re_encoded);
    std::int32_t re_encoded_value = 0;
    std::memcpy(&re_encoded_value, re_encoded, 4);
    EXPECT_NEAR(re_encoded_value, half_scale_positive, 2); // rounding tolerance
}

TEST(AsioSampleConverter, AsioSampleConverterInt32ClampsFullScaleNegative)
{
    const std::int32_t full_scale_negative = -2147483647 - 1; // INT32_MIN, avoids UB literal
    std::uint8_t bytes[4];
    std::memcpy(bytes, &full_scale_negative, 4);

    const float decoded =
        SushiDSP::asio_sample_to_float(SushiDSP::AsioSampleFormat::Int32LSB, bytes);
    EXPECT_NEAR(decoded, -1.0f, 1e-6f);
}

TEST(AsioSampleConverter, AsioSampleConverterFloat32PassesThroughUnchanged)
{
    const float original = -0.375f;
    std::uint8_t bytes[4];
    std::memcpy(bytes, &original, 4);

    const float decoded =
        SushiDSP::asio_sample_to_float(SushiDSP::AsioSampleFormat::Float32LSB, bytes);
    EXPECT_FLOAT_EQ(decoded, original);

    std::uint8_t re_encoded[4];
    SushiDSP::float_to_asio_sample(SushiDSP::AsioSampleFormat::Float32LSB, 0.25f, re_encoded);
    float re_encoded_value = 0.0f;
    std::memcpy(&re_encoded_value, re_encoded, 4);
    EXPECT_FLOAT_EQ(re_encoded_value, 0.25f);
}

TEST(AsioSampleConverter, AsioSampleConverterInt16RoundTripsKnownValues)
{
    const std::int16_t quarter_scale = 8192; // 0.25 * 32768
    std::uint8_t bytes[2];
    std::memcpy(bytes, &quarter_scale, 2);

    const float decoded =
        SushiDSP::asio_sample_to_float(SushiDSP::AsioSampleFormat::Int16LSB, bytes);
    EXPECT_NEAR(decoded, 0.25f, 1e-4f);
}

TEST(AsioSampleConverter, AsioSampleConverterFloatToSampleClampsOutOfRangeInput)
{
    // Out-of-range input clamps to full scale. See tests/foundation/audio_format/README.md.
    std::uint8_t bytes[4];
    SushiDSP::float_to_asio_sample(SushiDSP::AsioSampleFormat::Int32LSB, 2.0f, bytes);
    std::int32_t decoded_value = 0;
    std::memcpy(&decoded_value, bytes, 4);
    EXPECT_EQ(decoded_value, 2147483647);
}
