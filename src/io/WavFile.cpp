/****************************************************************/
/* WavFile.cpp                                                  */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file WavFile.cpp
 * @brief Implements the RIFF/WAVE reader.
 * @author Mustafa Garip
 */

#include "SushiDSP/io/WavFile.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <stdexcept>

namespace SushiDSP
{
    namespace
    {
        uint32_t read_u32_le(const uint8_t* p) { return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24); }
        uint16_t read_u16_le(const uint8_t* p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }

        float pcm16_to_float(const uint8_t* p) { return static_cast<int16_t>(read_u16_le(p)) / 32768.0f; }

        float pcm24_to_float(const uint8_t* p)
        {
            int32_t value = static_cast<int32_t>(p[0]) | (static_cast<int32_t>(p[1]) << 8) | (static_cast<int32_t>(p[2]) << 16);
            if (value & 0x800000)
            {
                value |= static_cast<int32_t>(0xFF000000);
            }
            return static_cast<float>(value) / 8388608.0f;
        }

        float pcm32_to_float(const uint8_t* p)
        {
            const int32_t value = static_cast<int32_t>(read_u32_le(p));
            return static_cast<float>(value) / 2147483648.0f;
        }

        float ieee_float32_to_float(const uint8_t* p)
        {
            float value;
            std::memcpy(&value, p, sizeof(float));
            return value;
        }
    }

    std::vector<std::vector<float>> WavFile::load_channels(const std::string& path, double* out_sample_rate)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file)
        {
            throw std::runtime_error("WavFile: could not open '" + path + "'");
        }

        std::vector<uint8_t> bytes(
            (std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

        if (bytes.size() < 44)
        {
            throw std::runtime_error("WavFile: '" + path + "' is too small to be a valid WAV file");
        }
        if (std::memcmp(bytes.data(), "RIFF", 4) != 0 || std::memcmp(bytes.data() + 8, "WAVE", 4) != 0)
        {
            throw std::runtime_error("WavFile: '" + path + "' is not a RIFF/WAVE file");
        }

        std::uint16_t format_tag = 0;
        std::uint16_t channel_count = 0;
        std::uint32_t sample_rate = 0;
        std::uint16_t bits_per_sample = 0;
        const uint8_t* data_start = nullptr;
        std::uint32_t data_bytes = 0;

        std::size_t offset = 12;
        while (offset + 8 <= bytes.size())
        {
            char chunk_id[5] = { 0 };
            std::memcpy(chunk_id, bytes.data() + offset, 4);
            const uint32_t chunk_size = read_u32_le(bytes.data() + offset + 4);
            const std::size_t chunk_data_offset = offset + 8;

            if (chunk_data_offset + chunk_size > bytes.size())
            {
                throw std::runtime_error("WavFile: '" + path + "' has a truncated chunk");
            }

            if (std::memcmp(chunk_id, "fmt ", 4) == 0)
            {
                if (chunk_size < 16)
                {
                    throw std::runtime_error("WavFile: '" + path + "' has a malformed fmt chunk");
                }
                const uint8_t* fmt = bytes.data() + chunk_data_offset;
                format_tag = read_u16_le(fmt + 0);
                channel_count = read_u16_le(fmt + 2);
                sample_rate = read_u32_le(fmt + 4);
                bits_per_sample = read_u16_le(fmt + 14);
            }
            else if (std::memcmp(chunk_id, "data", 4) == 0)
            {
                data_start = bytes.data() + chunk_data_offset;
                data_bytes = chunk_size;
            }

            offset = chunk_data_offset + chunk_size + (chunk_size % 2);
        }

        if (format_tag != 1 && format_tag != 3)
        {
            throw std::runtime_error("WavFile: '" + path + "' uses an unsupported format tag (only PCM and IEEE float are supported)");
        }
        if (channel_count == 0 || data_start == nullptr)
        {
            throw std::runtime_error("WavFile: '" + path + "' is missing fmt or data chunk content");
        }

        const std::uint32_t bytes_per_sample = bits_per_sample / 8;
        const std::uint32_t frame_bytes = bytes_per_sample * channel_count;
        const std::uint32_t frame_count = frame_bytes > 0 ? data_bytes / frame_bytes : 0;

        std::vector<std::vector<float>> channels(channel_count, std::vector<float>(frame_count, 0.0f));
        for (std::uint32_t frame = 0; frame < frame_count; ++frame)
        {
            for (std::uint16_t channel = 0; channel < channel_count; ++channel)
            {
                const uint8_t* sample_ptr = data_start + frame * frame_bytes + channel * bytes_per_sample;
                float sample_value = 0.0f;
                if (format_tag == 3 && bits_per_sample == 32)
                {
                    sample_value = ieee_float32_to_float(sample_ptr);
                }
                else if (format_tag == 1 && bits_per_sample == 16)
                {
                    sample_value = pcm16_to_float(sample_ptr);
                }
                else if (format_tag == 1 && bits_per_sample == 24)
                {
                    sample_value = pcm24_to_float(sample_ptr);
                }
                else if (format_tag == 1 && bits_per_sample == 32)
                {
                    sample_value = pcm32_to_float(sample_ptr);
                }
                else
                {
                    throw std::runtime_error("WavFile: '" + path + "' uses an unsupported bit depth for its format tag");
                }
                channels[channel][frame] = sample_value;
            }
        }

        if (out_sample_rate != nullptr)
        {
            *out_sample_rate = static_cast<double>(sample_rate);
        }

        return channels;
    }

    std::vector<float> WavFile::load_mono(const std::string& path, double* out_sample_rate)
    {
        const std::vector<std::vector<float>> channels = load_channels(path, out_sample_rate);
        const std::size_t                     frame_count = channels[0].size();

        std::vector<float> mono(frame_count, 0.0f);
        for (std::size_t frame = 0; frame < frame_count; ++frame)
        {
            float sum = 0.0f;
            for (const std::vector<float>& channel_samples : channels)
            {
                sum += channel_samples[frame];
            }
            mono[frame] = sum / static_cast<float>(channels.size());
        }
        return mono;
    }
}
