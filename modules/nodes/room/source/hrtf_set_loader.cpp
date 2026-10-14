/**************************************************************************/
/* hrtf_set_loader.cpp                                                    */
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
 * @file hrtf_set_loader.cpp
 * @brief Validates the asset's header against the file's own length before trusting any count in it.
 * @author Mustafa Garip & Sushi Systems
 */

#include <SushiDSP/nodes/room/hrtf_set_loader.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <exception>
#include <fstream>
#include <ios>
#include <sstream>
#include <vector>

namespace SushiDSP::room
{
    namespace
    {
        /// @brief Stores the exact bytes convert_hrtf.py writes at offset 0.
        constexpr char K_MAGIC[8] = {'S', 'D', 'H', 'R', 'T', 'F', '\0', '\0'};

        /// @brief Stores the only format version this loader accepts.
        constexpr std::uint32_t K_FORMAT_VERSION = 1;

        /// @brief Counts the bytes of the fixed header.
        constexpr std::uint64_t K_HEADER_BYTES = 32;

        /// @brief Holds the fixed header, unpacked from little-endian bytes.
        struct Header
        {
                std::uint32_t version;
                std::uint32_t sample_rate_hz;
                std::uint32_t direction_count;
                std::uint32_t tap_count;
                std::uint32_t decoder_order;
                std::uint32_t flags;
        };

        /// @brief Reads one little-endian uint32_t from a buffer.
        std::uint32_t read_u32(const unsigned char* bytes) noexcept
        {
            return static_cast<std::uint32_t>(bytes[0]) |
                   (static_cast<std::uint32_t>(bytes[1]) << 8) |
                   (static_cast<std::uint32_t>(bytes[2]) << 16) |
                   (static_cast<std::uint32_t>(bytes[3]) << 24);
        }

        /// @brief Reads one little-endian float from a buffer.
        float read_f32(const unsigned char* bytes) noexcept
        {
            static_assert(sizeof(float) == 4, "HrtfSetLoader assumes a 32-bit float");
            std::array<unsigned char, 4> raw = {bytes[0], bytes[1], bytes[2], bytes[3]};
            float value;
            std::memcpy(&value, raw.data(), sizeof(value));
            return value;
        }

        /// @brief Reads count floats, little-endian, starting at cursor, and advances cursor.
        void read_floats(const unsigned char* data, std::size_t& cursor, std::uint32_t count,
                         std::vector<float>& out)
        {
            out.resize(count);
            for (std::uint32_t index = 0; index < count; ++index)
            {
                out[index] = read_f32(data + cursor);
                cursor += 4;
            }
        }

        /// @brief Returns the file's whole content, or an empty, unset optional-like pair on any
        /// failure to open or read it.
        /// @return False when the path cannot be opened for reading.
        bool read_whole_file(const std::string& path, std::vector<unsigned char>& bytes)
        {
            std::ifstream stream(path, std::ios::binary | std::ios::ate);
            if (!stream.is_open())
            {
                return false;
            }
            const std::streampos length = stream.tellg();
            if (length < 0)
            {
                return false;
            }
            bytes.resize(static_cast<std::size_t>(length));
            stream.seekg(0, std::ios::beg);
            if (!bytes.empty())
            {
                stream.read(reinterpret_cast<char*>(bytes.data()),
                            static_cast<std::streamsize>(bytes.size()));
            }
            return stream.good() || stream.eof();
        }

        /// @brief Returns the byte length the header implies the file must have, computed from
        /// the header's own counts so a truncated file is caught by comparing against what was
        /// actually read, never by trusting those counts on their own.
        std::uint64_t expected_length(const Header& header) noexcept
        {
            const std::uint64_t direction_record =
                12 + 2 * static_cast<std::uint64_t>(header.tap_count) * 4;
            const std::uint64_t channels =
                static_cast<std::uint64_t>(header.decoder_order + 1) * (header.decoder_order + 1);
            const std::uint64_t decoder_bytes = 2 * channels * HrtfSet::K_DECODER_TAPS * 4;
            const std::uint64_t diffuse_bytes = 2 * HrtfSet::K_DIFFUSE_TAPS * 4;
            return K_HEADER_BYTES +
                   static_cast<std::uint64_t>(header.direction_count) * direction_record +
                   decoder_bytes + diffuse_bytes;
        }
    } // namespace

    bool HrtfSetLoader::load(const std::string& path, double expected_sample_rate, HrtfSet& set,
                             std::string& error_out)
    {
        std::vector<unsigned char> bytes;
        if (!read_whole_file(path, bytes))
        {
            error_out = "HrtfSetLoader: cannot open '" + path + "'";
            return false;
        }
        if (bytes.size() < K_HEADER_BYTES)
        {
            error_out = "HrtfSetLoader: '" + path + "' is too short to hold a header";
            return false;
        }
        if (std::memcmp(bytes.data(), K_MAGIC, sizeof(K_MAGIC)) != 0)
        {
            error_out = "HrtfSetLoader: '" + path + "' has the wrong magic";
            return false;
        }
        Header header{};
        header.version = read_u32(bytes.data() + 8);
        header.sample_rate_hz = read_u32(bytes.data() + 12);
        header.direction_count = read_u32(bytes.data() + 16);
        header.tap_count = read_u32(bytes.data() + 20);
        header.decoder_order = read_u32(bytes.data() + 24);
        header.flags = read_u32(bytes.data() + 28);
        if (header.version != K_FORMAT_VERSION)
        {
            std::ostringstream message;
            message << "HrtfSetLoader: '" << path << "' is format version " << header.version
                    << ", expected " << K_FORMAT_VERSION;
            error_out = message.str();
            return false;
        }
        if (header.tap_count != HrtfSet::K_TAP_COUNT)
        {
            std::ostringstream message;
            message << "HrtfSetLoader: '" << path << "' carries " << header.tap_count
                    << " taps per HRIR, expected " << HrtfSet::K_TAP_COUNT;
            error_out = message.str();
            return false;
        }
        if (static_cast<double>(header.sample_rate_hz) != expected_sample_rate)
        {
            std::ostringstream message;
            message << "HrtfSetLoader: '" << path << "' was measured at " << header.sample_rate_hz
                    << " Hz, but " << expected_sample_rate << " Hz was asked for";
            error_out = message.str();
            return false;
        }
        if (bytes.size() != expected_length(header))
        {
            error_out = "HrtfSetLoader: '" + path + "' is truncated or malformed";
            return false;
        }

        std::size_t cursor = K_HEADER_BYTES;
        std::vector<HrtfSet::Direction> directions(header.direction_count);
        std::vector<float> left_taps;
        std::vector<float> right_taps;
        left_taps.reserve(static_cast<std::size_t>(header.direction_count) * header.tap_count);
        right_taps.reserve(static_cast<std::size_t>(header.direction_count) * header.tap_count);
        for (std::uint32_t index = 0; index < header.direction_count; ++index)
        {
            HrtfSet::Direction direction{};
            direction.azimuth_rad = read_f32(bytes.data() + cursor);
            direction.elevation_rad = read_f32(bytes.data() + cursor + 4);
            direction.itd_frames = read_f32(bytes.data() + cursor + 8);
            cursor += 12;
            std::vector<float> left;
            std::vector<float> right;
            read_floats(bytes.data(), cursor, header.tap_count, left);
            read_floats(bytes.data(), cursor, header.tap_count, right);
            directions[index] = direction;
            left_taps.insert(left_taps.end(), left.begin(), left.end());
            right_taps.insert(right_taps.end(), right.begin(), right.end());
        }

        const std::uint32_t channels = (header.decoder_order + 1) * (header.decoder_order + 1);
        std::vector<float> decoder_filters;
        read_floats(bytes.data(), cursor, 2 * channels * HrtfSet::K_DECODER_TAPS, decoder_filters);

        std::vector<float> diffuse_left_vector;
        std::vector<float> diffuse_right_vector;
        read_floats(bytes.data(), cursor, HrtfSet::K_DIFFUSE_TAPS, diffuse_left_vector);
        read_floats(bytes.data(), cursor, HrtfSet::K_DIFFUSE_TAPS, diffuse_right_vector);

        // A failure leaves the caller's set as it was; see modules/nodes/room/README.md.
        try
        {
            HrtfSet loaded(static_cast<double>(header.sample_rate_hz), std::move(directions),
                           std::move(left_taps), std::move(right_taps));
            loaded.set_decoder(header.decoder_order, std::move(decoder_filters));
            std::array<float, HrtfSet::K_DIFFUSE_TAPS> diffuse_left{};
            std::array<float, HrtfSet::K_DIFFUSE_TAPS> diffuse_right{};
            std::copy(diffuse_left_vector.begin(), diffuse_left_vector.end(), diffuse_left.begin());
            std::copy(diffuse_right_vector.begin(), diffuse_right_vector.end(),
                      diffuse_right.begin());
            loaded.set_diffuse_field(diffuse_left, diffuse_right);
            set = std::move(loaded);
        }
        catch (const std::exception& error)
        {
            error_out = "HrtfSetLoader: '" + path + "' " + std::string(error.what());
            return false;
        }
        return true;
    }
} // namespace SushiDSP::room
