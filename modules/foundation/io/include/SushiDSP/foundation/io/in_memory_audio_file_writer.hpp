/**************************************************************************/
/* in_memory_audio_file_writer.hpp                                        */
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
 * @file in_memory_audio_file_writer.hpp
 * @brief Declares the in-memory audio file writer and the store that opens it by path.
 * @author Mustafa Garip
 */

#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include <SushiDSP/foundation/io/audio_file_writer.hpp>

namespace SushiDSP
{
    /// @brief What an in-memory file holds: the shape it was opened with and the samples
    /// appended, one vector per channel.
    struct InMemoryRecording
    {
        double sample_rate = 0.0;
        std::uint32_t channels = 0;
        std::vector<std::vector<float>> samples;
        bool closed = false;

        /// @brief Returns the frames held per channel.
        std::uint64_t frames() const
        {
            return samples.empty() ? 0u : samples[0].size();
        }
    };

    /// @brief Appends into an InMemoryRecording it does not own. The recording must outlive it.
    class InMemoryAudioFileWriter final : public AudioFileWriter
    {
        public:
            explicit InMemoryAudioFileWriter(InMemoryRecording& recording) : recording_(recording)
            {
                recording_.samples.assign(recording_.channels, std::vector<float>());
            }

            void append(const float* const* channels, std::uint32_t frames) override
            {
                if (recording_.closed)
                {
                    return;
                }
                for (std::uint32_t channel = 0; channel < recording_.channels; ++channel)
                {
                    recording_.samples[channel].insert(recording_.samples[channel].end(),
                                                       channels[channel], channels[channel] + frames);
                }
            }

            void close() override
            {
                recording_.closed = true;
            }

            std::uint64_t frames_written() const override
            {
                return recording_.frames();
            }

        private:
            InMemoryRecording& recording_;
    };

    /// @brief Holds one InMemoryRecording per opened path and hands out writers into them.
    ///
    /// The store must outlive every writer it opened. A test reads a recording after the
    /// recorder that wrote it has stopped, since a recorder's writer thread is the only one that
    /// appends and stop() joins it.
    class InMemoryAudioFileStore
    {
        public:
            /// @brief Returns an opener that creates a recording in this store, or throws
            /// std::runtime_error for a path in the refused set.
            AudioFileWriterOpener opener()
            {
                return [this](const std::string& path, double sample_rate, std::uint32_t channels)
                {
                    if (refused_.count(path) != 0)
                    {
                        throw std::runtime_error("in-memory store refuses to open '" + path + "'");
                    }
                    InMemoryRecording& recording = recordings_[path];
                    recording = InMemoryRecording();
                    recording.sample_rate = sample_rate;
                    recording.channels = channels;
                    return std::unique_ptr<AudioFileWriter>(new InMemoryAudioFileWriter(recording));
                };
            }

            /// @brief Makes every later open of `path` throw, as an unwritable folder does.
            void refuse(const std::string& path)
            {
                refused_.insert(path);
            }

            /// @brief Returns the recording opened at `path`, or nullptr if none was.
            const InMemoryRecording* recording(const std::string& path) const
            {
                const auto found = recordings_.find(path);
                return found == recordings_.end() ? nullptr : &found->second;
            }

        private:
            std::map<std::string, InMemoryRecording> recordings_;
            std::set<std::string> refused_;
    };
} // namespace SushiDSP
