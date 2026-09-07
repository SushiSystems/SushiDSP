/**************************************************************************/
/* WavWriter.cpp                                                          */
/**************************************************************************/
/*                         This file is part of:                          */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                        https://sushisystems.io                         */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License");        */
/* you may not use this file except in compliance with the License.       */
/* You may obtain a copy of the License at                                */
/*                                                                        */
/*     http://www.apache.org/licenses/LICENSE-2.0                         */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

#include "SushiDSP/io/WavWriter.hpp"

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
            std::uint8_t bytes[4] = {
                static_cast<std::uint8_t>(value & 0xFF),
                static_cast<std::uint8_t>((value >> 8) & 0xFF),
                static_cast<std::uint8_t>((value >> 16) & 0xFF),
                static_cast<std::uint8_t>((value >> 24) & 0xFF)
            };
            file.write(reinterpret_cast<const char*>(bytes), sizeof(bytes));
        }

        void write_u16_le(std::ofstream& file, std::uint16_t value)
        {
            std::uint8_t bytes[2] = {
                static_cast<std::uint8_t>(value & 0xFF),
                static_cast<std::uint8_t>((value >> 8) & 0xFF)
            };
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
    }

    WavWriter::WavWriter(const std::string& path, double sample_rate, std::uint32_t channels, Format format)
        : m_file(path, std::ios::binary | std::ios::trunc), m_channels(channels), m_format(format)
    {
        if (!m_file)
        {
            throw std::runtime_error("WavWriter: could not open '" + path + "' for writing");
        }

        const std::uint16_t bits = bits_per_sample(m_format);
        const std::uint32_t byte_rate =
            static_cast<std::uint32_t>(sample_rate) * m_channels * (bits / 8);
        const std::uint16_t block_align = static_cast<std::uint16_t>(m_channels * (bits / 8));

        write_tag(m_file, "RIFF");
        write_u32_le(m_file, 0); // Placeholder RIFF size, rewritten in close().
        write_tag(m_file, "WAVE");
        write_tag(m_file, "fmt ");
        write_u32_le(m_file, 16);
        write_u16_le(m_file, format_tag(m_format));
        write_u16_le(m_file, static_cast<std::uint16_t>(m_channels));
        write_u32_le(m_file, static_cast<std::uint32_t>(sample_rate));
        write_u32_le(m_file, byte_rate);
        write_u16_le(m_file, block_align);
        write_u16_le(m_file, bits);
        write_tag(m_file, "data");
        write_u32_le(m_file, 0); // Placeholder data size, rewritten in close().
    }

    WavWriter::~WavWriter()
    {
        close();
    }

    void WavWriter::append(const float* const* channels, std::uint32_t frames)
    {
        const std::uint16_t bits = bits_per_sample(m_format);
        for (std::uint32_t frame = 0; frame < frames; ++frame)
        {
            for (std::uint32_t channel = 0; channel < m_channels; ++channel)
            {
                const float sample = channels[channel][frame];
                if (m_format == Format::Pcm16)
                {
                    const std::int16_t pcm = clamp_to_pcm16(sample);
                    write_u16_le(m_file, static_cast<std::uint16_t>(pcm));
                }
                else
                {
                    static_assert(sizeof(float) == 4, "Float32 WAV output assumes a 4-byte float");
                    m_file.write(reinterpret_cast<const char*>(&sample), sizeof(sample));
                }
            }
        }
        (void)bits;
        m_frames_written += frames;
    }

    void WavWriter::close()
    {
        if (m_closed)
        {
            return;
        }

        const std::uint16_t bits = bits_per_sample(m_format);
        const std::uint32_t data_bytes =
            static_cast<std::uint32_t>(m_frames_written) * m_channels * (bits / 8);
        const std::uint32_t riff_size = 36 + data_bytes;

        m_file.seekp(4);
        write_u32_le(m_file, riff_size);
        m_file.seekp(40);
        write_u32_le(m_file, data_bytes);

        m_file.close();
        m_closed = true;
    }

    std::uint64_t WavWriter::frames_written() const
    {
        return m_frames_written;
    }
}
