/****************************************************************/
/* AsioSampleConverter.cpp                                      */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file AsioSampleConverter.cpp
 * @brief Implements the conversions between hardware sample formats and float.
 * @author Mustafa Garip
 */

#include "SushiDSP/audio_format/AsioSampleConverter.hpp"

#include <algorithm>
#include <cstring>

namespace SushiDSP
{
    namespace
    {
        constexpr float kInt16FullScale = 32768.0f;
        constexpr float kInt32FullScale = 2147483648.0f;

        float clamp_unit(float value)
        {
            return std::max(-1.0f, std::min(1.0f, value));
        }
    }

    float asio_sample_to_float(AsioSampleFormat format, const void* sample_bytes)
    {
        switch (format)
        {
            case AsioSampleFormat::Int16LSB:
            {
                std::int16_t raw = 0;
                std::memcpy(&raw, sample_bytes, sizeof(raw));
                return static_cast<float>(raw) / kInt16FullScale;
            }
            case AsioSampleFormat::Int32LSB:
            {
                std::int32_t raw = 0;
                std::memcpy(&raw, sample_bytes, sizeof(raw));
                return static_cast<float>(raw) / kInt32FullScale;
            }
            case AsioSampleFormat::Float32LSB:
            {
                float raw = 0.0f;
                std::memcpy(&raw, sample_bytes, sizeof(raw));
                return raw;
            }
        }
        return 0.0f;
    }

    void float_to_asio_sample(AsioSampleFormat format, float value, void* out_sample_bytes)
    {
        const float clamped = clamp_unit(value);
        switch (format)
        {
            case AsioSampleFormat::Int16LSB:
            {
                const std::int16_t raw = static_cast<std::int16_t>(clamped * (kInt16FullScale - 1.0f));
                std::memcpy(out_sample_bytes, &raw, sizeof(raw));
                return;
            }
            case AsioSampleFormat::Int32LSB:
            {
                // The scale is formed in double; in float it rounds up to 2^31 (src/README.md).
                const std::int32_t raw = static_cast<std::int32_t>(clamped * (static_cast<double>(kInt32FullScale) - 1.0));
                std::memcpy(out_sample_bytes, &raw, sizeof(raw));
                return;
            }
            case AsioSampleFormat::Float32LSB:
            {
                std::memcpy(out_sample_bytes, &clamped, sizeof(clamped));
                return;
            }
        }
    }
}
