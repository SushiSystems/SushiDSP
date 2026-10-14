/**************************************************************************/
/* asio_sample_converter.cpp                                              */
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
 * @file asio_sample_converter.cpp
 * @brief Implements the conversions between hardware sample formats and float.
 * @author Mustafa Garip
 */

#include <SushiDSP/foundation/audio_format/asio_sample_converter.hpp>

#include <algorithm>
#include <cstring>

namespace SushiDSP
{
    namespace
    {
        constexpr float K_INT16_FULL_SCALE = 32768.0f;
        constexpr float K_INT32_FULL_SCALE = 2147483648.0f;

        float clamp_unit(float value)
        {
            return std::max(-1.0f, std::min(1.0f, value));
        }
    } // namespace

    float asio_sample_to_float(AsioSampleFormat format, const void* sample_bytes)
    {
        switch (format)
        {
        case AsioSampleFormat::Int16LSB:
        {
            std::int16_t raw = 0;
            std::memcpy(&raw, sample_bytes, sizeof(raw));
            return static_cast<float>(raw) / K_INT16_FULL_SCALE;
        }
        case AsioSampleFormat::Int32LSB:
        {
            std::int32_t raw = 0;
            std::memcpy(&raw, sample_bytes, sizeof(raw));
            return static_cast<float>(raw) / K_INT32_FULL_SCALE;
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
            const std::int16_t raw =
                static_cast<std::int16_t>(clamped * (K_INT16_FULL_SCALE - 1.0f));
            std::memcpy(out_sample_bytes, &raw, sizeof(raw));
            return;
        }
        case AsioSampleFormat::Int32LSB:
        {
            // The scale is formed in double; in float it rounds up to 2^31 (modules/foundation/audio_format/README.md).
            const std::int32_t raw = static_cast<std::int32_t>(
                clamped * (static_cast<double>(K_INT32_FULL_SCALE) - 1.0));
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
} // namespace SushiDSP
