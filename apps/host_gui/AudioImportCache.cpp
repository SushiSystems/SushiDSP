/**************************************************************************/
/* AudioImportCache.cpp                                                   */
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
 * @file AudioImportCache.cpp
 * @brief Defines the folder of decoded WAVs the host keeps beside its executable.
 * @author Mustafa Garip & Sushi Systems
 */

#include "AudioImportCache.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "SushiDSP/io/WavWriter.hpp"

#include "ChannelResampler.hpp"
#include "Mp3Decoder.hpp"

namespace SushiDSP::host
{
    namespace
    {
        /// @brief How many frames one append to the WAV writer carries.
        constexpr std::uint32_t kBlockFrames = 4096;

        /// @brief How many characters of the source's own stem the entry name keeps.
        constexpr std::size_t kStemLimit = 48;

        /// @brief The extension this cache decodes.
        constexpr const char* kSourceExtension = ".mp3";

        /// @brief The digits an FNV-1a hash is written in.
        constexpr const char* kHexDigits = "0123456789abcdef";

        /// @brief Lowercases a string one byte at a time.
        /// @param text The string.
        /// @return The lowercased copy.
        std::string lowercased(const std::string& text)
        {
            std::string out = text;
            std::transform(out.begin(), out.end(), out.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return out;
        }

        /// @brief Keeps the characters a file name may carry on every filesystem, so an entry
        /// named after a song still opens through the narrow stream WavWriter uses.
        /// @param stem The source file's stem.
        /// @return The stem with every other character turned into an underscore, cut to
        /// kStemLimit characters.
        std::string sanitized_stem(const std::string& stem)
        {
            std::string out;
            for (const char character : stem)
            {
                if (out.size() >= kStemLimit)
                {
                    break;
                }
                const unsigned char byte = static_cast<unsigned char>(character);
                const bool keep = std::isalnum(byte) != 0 || character == '-' || character == '_';
                out.push_back(keep ? character : '_');
            }
            return out.empty() ? std::string("take") : out;
        }

        /// @brief Hashes a string with FNV-1a, 64 bits.
        /// @param text The string to hash.
        /// @return The hash as sixteen lowercase hex digits.
        std::string hashed(const std::string& text)
        {
            std::uint64_t hash = 1469598103934665603ull;
            for (const char character : text)
            {
                hash ^= static_cast<std::uint64_t>(static_cast<unsigned char>(character));
                hash *= 1099511628211ull;
            }

            std::string digits(16, '0');
            for (int index = 15; index >= 0; --index)
            {
                digits[static_cast<std::size_t>(index)] = kHexDigits[hash & 0xfull];
                hash >>= 4;
            }
            return digits;
        }

        /// @brief Describes a source file by what would change if it were replaced.
        /// @param source_path The file.
        /// @return Its path, size and last write time joined; the size and the time read zero
        /// when the file cannot be examined.
        std::string source_identity(const std::string& source_path)
        {
            const std::filesystem::path file(source_path);

            std::error_code      size_failure;
            const std::uintmax_t size = std::filesystem::file_size(file, size_failure);

            std::error_code time_failure;
            const std::filesystem::file_time_type written =
                std::filesystem::last_write_time(file, time_failure);

            const unsigned long long bytes =
                size_failure ? 0ull : static_cast<unsigned long long>(size);
            const long long stamp = time_failure
                ? 0ll
                : static_cast<long long>(written.time_since_epoch().count());
            return source_path + "|" + std::to_string(bytes) + "|" + std::to_string(stamp);
        }

        /// @brief Writes planar channels to a WAV as 32-bit float, the shape the decoder and the
        /// resampler already carry.
        /// @param path Where to write.
        /// @param channels One vector of samples per channel; every channel is the same length.
        /// @param sample_rate The rate to record in the header, in Hz.
        /// @throws std::runtime_error when the file cannot be opened for writing.
        void write_wav(const std::string& path, const std::vector<std::vector<float>>& channels,
                       double sample_rate)
        {
            SushiDSP::WavWriter writer(path, sample_rate,
                                       static_cast<std::uint32_t>(channels.size()),
                                       SushiDSP::WavWriter::Format::Float32);

            std::vector<const float*> heads(channels.size(), nullptr);
            const std::size_t         frames = channels.front().size();
            for (std::size_t position = 0; position < frames; position += kBlockFrames)
            {
                const std::uint32_t block = static_cast<std::uint32_t>(
                    std::min<std::size_t>(kBlockFrames, frames - position));
                for (std::size_t channel = 0; channel < channels.size(); ++channel)
                {
                    heads[channel] = channels[channel].data() + position;
                }
                writer.append(heads.data(), block);
            }
            writer.close();
        }
    }

    AudioImportCache::AudioImportCache(std::string directory)
        : directory_(std::move(directory))
    {
    }

    bool AudioImportCache::can_import(const std::string& path)
    {
        return lowercased(std::filesystem::path(path).extension().string()) == kSourceExtension;
    }

    std::string AudioImportCache::entry_name(const std::string& source_path, double target_rate)
    {
        const std::filesystem::path source(source_path);
        const long long rate = static_cast<long long>(target_rate > 0.0 ? target_rate : 0.0);
        return sanitized_stem(source.stem().string()) + "_" + hashed(source_identity(source_path))
            + "_" + std::to_string(rate) + ".wav";
    }

    std::string AudioImportCache::entry_path(const std::string& source_path,
                                             double target_rate) const
    {
        return (std::filesystem::path(directory_) / entry_name(source_path, target_rate)).string();
    }

    std::string AudioImportCache::import(const std::string& source_path, double target_rate) const
    {
        const std::string destination = entry_path(source_path, target_rate);
        std::error_code   failure;
        if (std::filesystem::exists(destination, failure))
        {
            return destination;
        }

        std::filesystem::create_directories(directory_, failure);
        if (failure)
        {
            throw std::runtime_error("cannot create the import cache at " + directory_ + ": "
                                     + failure.message());
        }

        const DecodedAudio decoded = decode_mp3(source_path);
        const double       written_rate = target_rate > 0.0 ? target_rate : decoded.sample_rate;
        const std::vector<std::vector<float>> converted =
            resample_channels(decoded.channels, decoded.sample_rate, written_rate);

        const std::string partial = destination + ".part";
        write_wav(partial, converted, written_rate);
        std::filesystem::rename(partial, destination, failure);
        if (failure)
        {
            std::error_code ignored;
            std::filesystem::remove(partial, ignored);
            throw std::runtime_error("cannot place the decoded WAV at " + destination + ": "
                                     + failure.message());
        }
        return destination;
    }

    const std::string& AudioImportCache::directory() const
    {
        return directory_;
    }
}
