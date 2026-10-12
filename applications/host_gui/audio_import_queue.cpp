/**************************************************************************/
/* audio_import_queue.cpp                                                 */
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
 * @file audio_import_queue.cpp
 * @brief Defines the worker that decodes a dropped song while the GUI keeps drawing.
 * @author Mustafa Garip & Sushi Systems
 */

#include "audio_import_queue.hpp"

#include <exception>
#include <iterator>
#include <utility>

namespace SushiDSP::host
{
    AudioImportQueue::AudioImportQueue(AudioImportCache cache)
        : cache_(std::move(cache)), worker_(
                                        [this]
                                        {
                                            run();
                                        })
    {
    }

    AudioImportQueue::~AudioImportQueue()
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            running_ = false;
        }
        wakeup_.notify_all();
        worker_.join();
    }

    void AudioImportQueue::request(std::string source_path, std::size_t track,
                                   std::uint64_t start_frame, double target_rate)
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            jobs_.push_back(Job{std::move(source_path), track, start_frame, target_rate});
        }
        wakeup_.notify_one();
    }

    void AudioImportQueue::drain(std::vector<AudioImport>& out)
    {
        std::vector<AudioImport> taken;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            taken.swap(finished_);
        }
        out.insert(out.end(), std::make_move_iterator(taken.begin()),
                   std::make_move_iterator(taken.end()));
    }

    std::size_t AudioImportQueue::pending() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return jobs_.size() + (decoding_.empty() ? 0u : 1u);
    }

    std::string AudioImportQueue::decoding() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return decoding_;
    }

    AudioImport AudioImportQueue::execute(const Job& job) const
    {
        AudioImport result;
        result.source_path = job.source_path;
        result.track = job.track;
        result.start_frame = job.start_frame;
        try
        {
            result.wav_path = cache_.import(job.source_path, job.target_rate);
        }
        catch (const std::exception& failure)
        {
            result.error = failure.what();
        }
        return result;
    }

    void AudioImportQueue::run()
    {
        for (;;)
        {
            Job job;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                wakeup_.wait(lock,
                             [this]
                             {
                                 return !running_ || !jobs_.empty();
                             });
                if (!running_)
                {
                    return;
                }
                job = std::move(jobs_.front());
                jobs_.pop_front();
                decoding_ = job.source_path;
            }

            AudioImport result = execute(job);

            {
                std::lock_guard<std::mutex> lock(mutex_);
                decoding_.clear();
                finished_.push_back(std::move(result));
            }
        }
    }
} // namespace SushiDSP::host
