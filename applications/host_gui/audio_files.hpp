/**************************************************************************/
/* audio_files.hpp                                                        */
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
 * @file audio_files.hpp
 * @brief Declares the port through which the session reads, writes and deletes audio files, and its
 * two adapters.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <SushiDSP/foundation/io/audio_file_writer.hpp>
#include <SushiDSP/foundation/io/in_memory_audio_file_writer.hpp>
#include <SushiDSP/foundation/io/wav_writer.hpp>
#include <SushiDSP/nodes/capture/take.hpp>

namespace SushiDSP::host
{
    /// @brief Port to the audio files of a session: a take is read from one, a recording is
    /// written into one and a deleted lane removes one. DiskAudioFiles is the adapter that touches
    /// the filesystem and InMemoryAudioFiles keeps the files in memory. Not real-time safe: call it
    /// from the control thread or a writer thread.
    class AudioFiles
    {
        public:
            virtual ~AudioFiles() = default;

            /// @brief Reads a WAV file into a take of the file's own channel count and rate.
            /// @param path The file's path.
            /// @return The take.
            /// @throws std::runtime_error when the file does not exist or does not parse.
            virtual SushiDSP::Take read(const std::string& path) const = 0;

            /// @brief Returns the opener a recorder or an export uses to create its file.
            /// @param format The sample encoding the file holds.
            /// @return An opener that creates or replaces the file at the path it is given, and
            /// throws std::runtime_error when it cannot.
            virtual SushiDSP::AudioFileWriterOpener writer_opener(
                SushiDSP::WavWriter::Format format = SushiDSP::WavWriter::Format::Float32) = 0;

            /// @brief Deletes a file.
            /// @param path The file's path.
            /// @return Empty when the file is gone, which includes a file that was never there;
            /// otherwise the reason it could not be deleted.
            virtual std::string remove(const std::string& path) = 0;
    };

    /// @brief Adapter that reads WAV files through Take::load, writes WAV files through
    /// WavWriter and deletes with the filesystem.
    class DiskAudioFiles final : public AudioFiles
    {
        public:
            SushiDSP::Take read(const std::string& path) const override
            {
                return SushiDSP::Take::load(path);
            }

            SushiDSP::AudioFileWriterOpener writer_opener(
                SushiDSP::WavWriter::Format format) override
            {
                return [format](const std::string& path, double sample_rate, std::uint32_t channels)
                {
                    return std::unique_ptr<SushiDSP::AudioFileWriter>(
                        new SushiDSP::WavWriter(path, sample_rate, channels, format));
                };
            }

            std::string remove(const std::string& path) override
            {
                std::error_code code;
                std::filesystem::remove(std::filesystem::u8path(path), code);
                return code ? code.message() : std::string();
            }
    };

    /// @brief Returns the adapter a session uses unless it is given another. It holds no state.
    /// @return The one DiskAudioFiles.
    inline AudioFiles& disk_audio_files()
    {
        static DiskAudioFiles files;
        return files;
    }

    /// @brief Adapter that holds every file in memory. A file is added by a test or written by a
    /// recorder, and read back as a take. A 16-bit file keeps the samples a WAV file of that
    /// encoding holds, so a round trip through it matches the disk adapter. Not thread safe: a
    /// writer thread appends into a file, so read it after the recorder has stopped.
    class InMemoryAudioFiles final : public AudioFiles
    {
        public:
            /// @brief Adds a file that reads back as the given audio.
            /// @param path The file's path.
            /// @param sample_rate The rate in Hz.
            /// @param channels One vector of samples per channel, all of the same length.
            void add(const std::string& path, double sample_rate,
                     const std::vector<std::vector<float>>& channels)
            {
                unreadable_.erase(path);
                std::unique_ptr<SushiDSP::AudioFileWriter> writer =
                    store_.opener()(path, sample_rate, static_cast<std::uint32_t>(channels.size()));
                std::vector<const float*> pointers;
                for (const std::vector<float>& channel : channels)
                {
                    pointers.push_back(channel.data());
                }
                const std::size_t frames = channels.empty() ? 0u : channels[0].size();
                writer->append(pointers.data(), static_cast<std::uint32_t>(frames));
                writer->close();
            }

            /// @brief Adds a file that exists and does not parse, so read() throws for it.
            /// @param path The file's path.
            void add_unreadable(const std::string& path)
            {
                store_.erase(path);
                unreadable_.insert(path);
            }

            /// @brief Makes every later open of a path throw, as an unwritable folder does.
            /// @param path The path whose open fails.
            void refuse_writing(const std::string& path)
            {
                store_.refuse(path);
            }

            /// @brief Reports whether a file exists, readable or not.
            /// @param path The file's path.
            bool exists(const std::string& path) const
            {
                return store_.recording(path) != nullptr || unreadable_.count(path) != 0u;
            }

            /// @brief Returns what a recorder or an add() put at a path.
            /// @param path The file's path.
            /// @return The recording, or nullptr when no file was written there.
            const SushiDSP::InMemoryRecording* recording(const std::string& path) const
            {
                return store_.recording(path);
            }

            SushiDSP::Take read(const std::string& path) const override
            {
                if (unreadable_.count(path) != 0u)
                {
                    throw std::runtime_error("in-memory file '" + path + "' is not a WAV file");
                }
                const SushiDSP::InMemoryRecording* file = store_.recording(path);
                if (file == nullptr)
                {
                    throw std::runtime_error("in-memory file '" + path + "' does not exist");
                }
                SushiDSP::Take take(file->channels, file->sample_rate,
                                    static_cast<std::size_t>(file->frames()));
                for (std::uint32_t channel = 0; channel < file->channels; ++channel)
                {
                    std::copy(file->samples[channel].begin(), file->samples[channel].end(),
                              take.mutable_channel(channel));
                }
                return take;
            }

            SushiDSP::AudioFileWriterOpener writer_opener(
                SushiDSP::WavWriter::Format format) override
            {
                const SushiDSP::AudioFileWriterOpener open_in_store = store_.opener();
                return [this, format, open_in_store](const std::string& path, double sample_rate,
                                                     std::uint32_t channels)
                {
                    unreadable_.erase(path);
                    std::unique_ptr<SushiDSP::AudioFileWriter> inner =
                        open_in_store(path, sample_rate, channels);
                    if (format == SushiDSP::WavWriter::Format::Float32)
                    {
                        return inner;
                    }
                    return std::unique_ptr<SushiDSP::AudioFileWriter>(
                        new Pcm16Writer(std::move(inner), channels));
                };
            }

            std::string remove(const std::string& path) override
            {
                store_.erase(path);
                unreadable_.erase(path);
                return std::string();
            }

        private:
            /// @brief Rounds each appended sample to the 16-bit value a WAV file of that encoding
            /// stores, then hands it on.
            class Pcm16Writer final : public SushiDSP::AudioFileWriter
            {
                public:
                    Pcm16Writer(std::unique_ptr<SushiDSP::AudioFileWriter> inner,
                                std::uint32_t channels)
                        : inner_(std::move(inner)), scratch_(channels)
                    {
                    }

                    void append(const float* const* channels, std::uint32_t frames) override
                    {
                        std::vector<const float*> pointers;
                        for (std::size_t channel = 0; channel < scratch_.size(); ++channel)
                        {
                            scratch_[channel].assign(channels[channel], channels[channel] + frames);
                            for (float& sample : scratch_[channel])
                            {
                                sample = static_cast<float>(std::lround(
                                             std::clamp(sample, -1.0f, 1.0f) * 32767.0f)) /
                                         32768.0f;
                            }
                            pointers.push_back(scratch_[channel].data());
                        }
                        inner_->append(pointers.data(), frames);
                    }

                    void close() override
                    {
                        inner_->close();
                    }

                    std::uint64_t frames_written() const override
                    {
                        return inner_->frames_written();
                    }

                private:
                    /// @brief Holds the writer the rounded samples go to.
                    std::unique_ptr<SushiDSP::AudioFileWriter> inner_;

                    /// @brief Holds the rounded samples of the block being appended, per channel.
                    std::vector<std::vector<float>> scratch_;
            };

            /// @brief Holds every file that parses.
            SushiDSP::InMemoryAudioFileStore store_;

            /// @brief Holds the paths whose file exists and does not parse.
            std::set<std::string> unreadable_;
    };
} // namespace SushiDSP::host
