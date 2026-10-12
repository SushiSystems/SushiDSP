/**************************************************************************/
/* audio_import_queue.hpp                                                 */
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
 * @file audio_import_queue.hpp
 * @brief Declares the worker that decodes a dropped song while the GUI keeps drawing.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "audio_import_cache.hpp"

namespace SushiDSP::host
{
    /// @brief One finished import, carrying the placement the drop asked for so the frame that
    /// picks it up can put the clip where the pointer was.
    struct AudioImport
    {
            /// @brief Holds the file that was dropped.
            std::string source_path;

            /// @brief Holds the WAV the source decoded into, empty when the import failed.
            std::string wav_path;

            /// @brief Holds the lane the clip belongs on.
            std::size_t track = 0;

            /// @brief Holds the frame the clip starts at.
            std::uint64_t start_frame = 0;

            /// @brief Holds the failure message, empty when the import succeeded.
            std::string error;
    };

    /// @brief Runs AudioImportCache::import() on one worker thread and hands the results back to
    /// the GUI frame, so a five-minute song decodes without the window stopping. Shaped like
    /// FileDropQueue: push work in, drain results out. A std::mutex is correct here because
    /// neither end runs on the audio thread.
    class AudioImportQueue
    {
        public:
            /// @brief Starts the worker.
            /// @param cache The cache every request is imported through.
            explicit AudioImportQueue(AudioImportCache cache);

            /// @brief Stops the worker, dropping whatever is still queued, and waits for the
            /// import in progress to finish.
            ~AudioImportQueue();

            AudioImportQueue(const AudioImportQueue&) = delete;
            AudioImportQueue& operator=(const AudioImportQueue&) = delete;

            /// @brief Queues one file for decoding.
            /// @param source_path The file to import.
            /// @param track The lane the finished clip belongs on.
            /// @param start_frame The frame the finished clip starts at.
            /// @param target_rate The rate to write the WAV at, in Hz.
            void request(std::string source_path, std::size_t track, std::uint64_t start_frame,
                         double target_rate);

            /// @brief Moves every finished import into out, in the order they finished, and empties
            /// the results.
            /// @param out Receives the finished imports; already-held entries are kept.
            void drain(std::vector<AudioImport>& out);

            /// @brief Counts the files queued and the one being decoded.
            std::size_t pending() const;

            /// @brief Names the file being decoded.
            /// @return The path, or an empty string when the worker is idle.
            std::string decoding() const;

        private:
            /// @brief One queued file and where its clip goes.
            struct Job
            {
                    /// @brief Holds the file to import.
                    std::string source_path;

                    /// @brief Holds the lane the finished clip belongs on.
                    std::size_t track = 0;

                    /// @brief Holds the frame the finished clip starts at.
                    std::uint64_t start_frame = 0;

                    /// @brief Holds the rate to write the WAV at, in Hz.
                    double target_rate = 0.0;
            };

            /// @brief Takes jobs until the queue is stopped, importing each and recording its result.
            void run();

            /// @brief Imports one job, turning any failure into a message.
            /// @param job The job to import.
            /// @return The finished import.
            AudioImport execute(const Job& job) const;

            /// @brief Holds the cache every request is imported through.
            AudioImportCache cache_;

            /// @brief Guards the jobs, the results, the running flag and the file being decoded.
            mutable std::mutex mutex_;

            /// @brief Wakes the worker when a job arrives or the queue stops.
            std::condition_variable wakeup_;

            /// @brief Holds the jobs waiting to be imported.
            std::deque<Job> jobs_;

            /// @brief Holds the imports that have finished and not yet been drained.
            std::vector<AudioImport> finished_;

            /// @brief Holds the file being decoded, empty while the worker is idle.
            std::string decoding_;

            /// @brief Holds whether the worker should keep taking jobs.
            bool running_ = true;

            /// @brief Holds the worker thread; declared last so every member it reads is built.
            std::thread worker_;
    };
} // namespace SushiDSP::host
