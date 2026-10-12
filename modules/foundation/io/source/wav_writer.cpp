/**************************************************************************/
/* wav_writer.cpp                                                         */
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
 * @file wav_writer.cpp
 * @brief Implements the streaming WAV writer.
 * @author Mustafa Garip
 */

#include "SushiDSP/io/wav_writer.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace SushiDSP
{
    namespace
    {
        void write_u32_le(std::ofstream& file, std::uint32_t value)
        {
            std::uint8_t bytes[4] = {static_cast<std::uint8_t>(value & 0xFF),
                                     static_cast<std::uint8_t>((value >> 8) & 0xFF),
                                     static_cast<std::uint8_t>((value >> 16) & 0xFF),
                                     static_cast<std::uint8_t>((value >> 24) & 0xFF)};
            file.write(reinterpret_cast<const char*>(bytes), sizeof(bytes));
        }

        void write_u16_le(std::ofstream& file, std::uint16_t value)
        {
            std::uint8_t bytes[2] = {static_cast<std::uint8_t>(value & 0xFF),
                                     static_cast<std::uint8_t>((value >> 8) & 0xFF)};
            file.write(reinterpret_cast<const char*>(bytes), sizeof(bytes));
        }

        void write_tag(std::ofstream& file, const char* tag)
        {
            file.write(tag, 4);
        }

        std::uint16_t bits_per_sample(WavWriter::Format format)
        {
            return format == WavWriter::Format::Pcm16 ? 16 : 32;
        }

        std::uint16_t format_tag(WavWriter::Format format)
        {
            return format == WavWriter::Format::Pcm16 ? 1 : 3;
        }

        std::int16_t clamp_to_pcm16(float sample)
        {
            const float clamped = std::max(-1.0f, std::min(1.0f, sample));
            return static_cast<std::int16_t>(std::lround(clamped * 32767.0f));
        }
    } // namespace

    namespace
    {
        constexpr std::uint32_t K_U32_MAX = 0xFFFFFFFFu;
        constexpr std::size_t K_PCM16_DATA_SIZE_OFFSET = 40;
        constexpr std::size_t K_FLOAT_FACT_SAMPLE_COUNT_OFFSET = 46;
        constexpr std::size_t K_FLOAT_DATA_SIZE_OFFSET = 54;
        constexpr std::uint32_t K_PCM16_RIFF_BASE = 36;
        constexpr std::uint32_t K_FLOAT_RIFF_BASE = 50;

        std::uint32_t clamp_to_u32(std::uint64_t value)
        {
            return value > K_U32_MAX ? K_U32_MAX : static_cast<std::uint32_t>(value);
        }
    } // namespace

    WavWriter::WavWriter(const std::string& path, double sample_rate, std::uint32_t channels,
                         Format format)
        : file_(path, std::ios::binary | std::ios::trunc), channels_(channels), format_(format)
    {
        if (!file_)
        {
            throw std::runtime_error("WavWriter: could not open '" + path + "' for writing");
        }

        const std::uint16_t bits = bits_per_sample(format_);
        const std::uint32_t byte_rate =
            static_cast<std::uint32_t>(sample_rate) * channels_ * (bits / 8);
        const std::uint16_t block_align = static_cast<std::uint16_t>(channels_ * (bits / 8));
        const bool is_float = format_ == Format::Float32;

        write_tag(file_, "RIFF");
        write_u32_le(file_, 0); // Placeholder RIFF size, rewritten in close().
        write_tag(file_, "WAVE");
        write_tag(file_, "fmt ");
        write_u32_le(file_, is_float ? 18 : 16);
        write_u16_le(file_, format_tag(format_));
        write_u16_le(file_, static_cast<std::uint16_t>(channels_));
        write_u32_le(file_, static_cast<std::uint32_t>(sample_rate));
        write_u32_le(file_, byte_rate);
        write_u16_le(file_, block_align);
        write_u16_le(file_, bits);
        if (is_float)
        {
            write_u16_le(file_, 0); // cbSize: no extra format bytes follow.
            write_tag(file_, "fact");
            write_u32_le(file_, 4);
            write_u32_le(file_, 0); // Placeholder sample count, rewritten in close().
        }
        write_tag(file_, "data");
        write_u32_le(file_, 0); // Placeholder data size, rewritten in close().
    }

    WavWriter::~WavWriter()
    {
        close();
    }

    void WavWriter::append(const float* const* channels, std::uint32_t frames)
    {
        if (closed_)
        {
            return;
        }

        for (std::uint32_t frame = 0; frame < frames; ++frame)
        {
            for (std::uint32_t channel = 0; channel < channels_; ++channel)
            {
                const float sample = channels[channel][frame];
                if (format_ == Format::Pcm16)
                {
                    const std::int16_t pcm = clamp_to_pcm16(sample);
                    write_u16_le(file_, static_cast<std::uint16_t>(pcm));
                }
                else
                {
                    static_assert(sizeof(float) == 4, "Float32 WAV output assumes a 4-byte float");
                    std::uint32_t bits_value = 0;
                    std::memcpy(&bits_value, &sample, sizeof(bits_value));
                    write_u32_le(file_, bits_value);
                }
            }
        }
        frames_written_ += frames;
    }

    void WavWriter::close()
    {
        if (closed_)
        {
            return;
        }

        const bool is_float = format_ == Format::Float32;
        const std::uint16_t bits = bits_per_sample(format_);
        const std::uint64_t data_bytes64 = frames_written_ * channels_ * (bits / 8);
        const std::uint32_t data_bytes = clamp_to_u32(data_bytes64);
        const std::uint32_t riff_base = is_float ? K_FLOAT_RIFF_BASE : K_PCM16_RIFF_BASE;
        const std::uint32_t riff_size =
            clamp_to_u32(static_cast<std::uint64_t>(riff_base) + data_bytes);

        file_.seekp(4);
        write_u32_le(file_, riff_size);
        if (is_float)
        {
            file_.seekp(K_FLOAT_FACT_SAMPLE_COUNT_OFFSET);
            write_u32_le(file_, clamp_to_u32(frames_written_));
        }
        file_.seekp(is_float ? K_FLOAT_DATA_SIZE_OFFSET : K_PCM16_DATA_SIZE_OFFSET);
        write_u32_le(file_, data_bytes);

        file_.close();
        closed_ = true;
    }

    std::uint64_t WavWriter::frames_written() const
    {
        return frames_written_;
    }
} // namespace SushiDSP
