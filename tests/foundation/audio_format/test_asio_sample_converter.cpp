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
/* Licensed under the Apache License, Version 2.0 (the "License"); you    */
/* may not use this file except in compliance with the License. You may   */
/* obtain a copy of the License at                                        */
/*                                                                        */
/*     https://www.apache.org/licenses/LICENSE-2.0                        */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

/**
 * @file test_asio_sample_converter.cpp
 * @brief Tests the conversion between ASIO sample formats and float.
 * @author Mustafa Garip
 */

#include <cstdint>
#include <cstring>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/allocation_trap/allocation_trap.hpp>
#include <SushiDSP/foundation/audio_format/asio_sample_converter.hpp>

namespace
{

/** Decodes the sample a format reads from the bytes of one integer container. */
template <typename Container>
float decode_container(SushiDSP::AsioSampleFormat format, Container raw)
{
    std::uint8_t bytes[sizeof(Container)];
    std::memcpy(bytes, &raw, sizeof(Container));
    return SushiDSP::asio_sample_to_float(format, bytes);
}

/** Encodes a float and reads back the integer container the format wrote. */
template <typename Container>
Container encode_container(SushiDSP::AsioSampleFormat format, float value)
{
    std::uint8_t bytes[sizeof(Container)];
    SushiDSP::float_to_asio_sample(format, value, bytes);
    Container raw = 0;
    std::memcpy(&raw, bytes, sizeof(Container));
    return raw;
}

// The encoders scale by full scale minus one and truncate: 0.5 gives 2^30 - 1, 0.25 gives 2^13 - 1.
constexpr std::int32_t kInt32HalfScale = 1073741824;
constexpr std::int32_t kInt32EncodedHalf = kInt32HalfScale - 1;
constexpr std::int32_t kInt32Max = 2147483647;
constexpr std::int32_t kInt32Min = -2147483647 - 1;
constexpr std::int16_t kInt16QuarterScale = 8192;
constexpr std::int16_t kInt16EncodedQuarter = kInt16QuarterScale - 1;

} // namespace

TEST(Small_AsioSampleConverter, AsioSampleConverterInt32DecodesHalfScaleToOneHalf)
{
    EXPECT_EQ(decode_container(SushiDSP::AsioSampleFormat::Int32LSB, kInt32HalfScale), 0.5f);
}

TEST(Small_AsioSampleConverter, AsioSampleConverterInt32EncodesOneHalfToTheTruncatedFullScaleFraction)
{
    EXPECT_EQ(encode_container<std::int32_t>(SushiDSP::AsioSampleFormat::Int32LSB, 0.5f), kInt32EncodedHalf);
}

TEST(Small_AsioSampleConverter, AsioSampleConverterInt32DecodesFullScaleNegativeToMinusOne)
{
    EXPECT_EQ(decode_container(SushiDSP::AsioSampleFormat::Int32LSB, kInt32Min), -1.0f);
}

TEST(Small_AsioSampleConverter, AsioSampleConverterInt32ClampsOutOfRangeNegativeInputToMinusFullScale)
{
    EXPECT_EQ(encode_container<std::int32_t>(SushiDSP::AsioSampleFormat::Int32LSB, -2.0f), -kInt32Max);
}

TEST(Small_AsioSampleConverter, AsioSampleConverterFloat32PassesThroughUnchanged)
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

TEST(Small_AsioSampleConverter, AsioSampleConverterInt16DecodesQuarterScaleToOneQuarter)
{
    EXPECT_EQ(decode_container(SushiDSP::AsioSampleFormat::Int16LSB, kInt16QuarterScale), 0.25f);
}

TEST(Small_AsioSampleConverter, AsioSampleConverterInt16EncodesOneQuarterToTheTruncatedFullScaleFraction)
{
    EXPECT_EQ(encode_container<std::int16_t>(SushiDSP::AsioSampleFormat::Int16LSB, 0.25f), kInt16EncodedQuarter);
}

TEST(Small_AsioSampleConverter, AsioSampleConverterFloatToSampleClampsOutOfRangeInput)
{
    // Out-of-range input clamps to full scale. See tests/foundation/audio_format/README.md.
    EXPECT_EQ(encode_container<std::int32_t>(SushiDSP::AsioSampleFormat::Int32LSB, 2.0f), kInt32Max);
}

TEST(Medium_AsioSampleConverter, ConversionPerformsNoHeapAllocation)
{
    std::uint8_t bytes[4] = {0, 0, 0, 0};
    float decoded = 0.0f;

    SushiDSP::test::AllocationTrap trap;
    trap.arm();
    decoded = SushiDSP::asio_sample_to_float(SushiDSP::AsioSampleFormat::Int32LSB, bytes);
    SushiDSP::float_to_asio_sample(SushiDSP::AsioSampleFormat::Int32LSB, decoded, bytes);
    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
}
