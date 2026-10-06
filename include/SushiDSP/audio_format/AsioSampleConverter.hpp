/**************************************************************************/
/* AsioSampleConverter.hpp                                                */
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
 * @file AsioSampleConverter.hpp
 * @brief Declares the hardware sample formats and their conversions to and from float.
 * @author Mustafa Garip
 */

#pragma once

#include <cstdint>

namespace SushiDSP
{
    /// @brief The sample container formats this converter supports, independent of any ASIO
    /// SDK type - AsioAudioDevice maps the real ASIOSampleType to this enum once at device-
    /// open time, keeping the ASIO SDK's headers out of every other translation unit that
    /// needs sample conversion (this file compiles and is tested on every platform).
    enum class AsioSampleFormat
    {
        Int16LSB,
        Int32LSB,
        Float32LSB,
    };

    /// @brief Decodes one sample from a hardware buffer into a float in [-1, 1].
    /// @param format The sample's container format.
    /// @param sample_bytes Pointer to the sample's raw bytes (2 or 4, per format).
    /// @return The decoded value.
    float asio_sample_to_float(AsioSampleFormat format, const void* sample_bytes);

    /// @brief Encodes a float in [-1, 1] into a hardware buffer's native format, clamping
    /// values outside that range to the format's full-scale value rather than overflowing.
    /// @param format The destination container format.
    /// @param value The value to encode.
    /// @param out_sample_bytes Destination for the sample's raw bytes (2 or 4, per format).
    void float_to_asio_sample(AsioSampleFormat format, float value, void* out_sample_bytes);
}
