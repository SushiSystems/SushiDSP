/**************************************************************************/
/* session_bounce.hpp                                                     */
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
 *  @file session_bounce.hpp
 *  @brief Declares the export: the session rendered offline into a WAV or an MP3 file.
 *  @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>

#include <SushiDSP/foundation/core/audio_node.hpp>

/// @brief Names the file an export writes.
enum class BounceFormat : std::uint8_t
{
    Wav16,      ///< A 16-bit PCM WAV.
    Wav32Float, ///< A 32-bit float WAV.
    Mp3         ///< An MP3 at a constant bit rate.
};

/// @brief Says what an export renders and where it writes it.
struct BounceRequest
{
        std::string session_json;                   ///< The session, as session_to_json() wrote it.
        std::string path;                           ///< The file to write.
        std::string hrtf_path;                      ///< The HRTF set the rooms use; empty for none.
        BounceFormat format = BounceFormat::Wav16;  ///< The file's kind.
        int mp3_kilobits_per_second = 320;          ///< The MP3's bit rate.
        SushiDSP::PrepareInfo info{48000.0, 256, 2}; ///< The rate and block the session runs at.
};

/// @brief Renders a session from frame zero to the end of its last clip and its tail, into a
/// file, on a thread of its own. It builds its own session from the request, so the device keeps
/// playing the live one. GUI thread, apart from the thread it starts.
/// @see docs/agent/2026_10_13_TRACK_RIGS_AND_PLUGIN/SPEC.md, section 4.5
class SessionBounce
{
    public:
        /// @brief Holds the level under which the tail counts as silent, linear: -90 dBFS.
        static constexpr float K_SILENCE = 3.1623e-5f;

        /// @brief Holds how long the output must stay silent for the tail to end, in seconds.
        static constexpr double K_SILENT_SECONDS = 0.5;

        /// @brief Bounds the tail, in seconds.
        static constexpr double K_MAX_TAIL_SECONDS = 10.0;

        SessionBounce() = default;

        /// @brief Waits for a running export to end.
        ~SessionBounce();
        SessionBounce(const SessionBounce&) = delete;
        SessionBounce& operator=(const SessionBounce&) = delete;

        /// @brief Starts an export. A second start while one runs is refused.
        /// @return False when an export is already running.
        bool start(BounceRequest request);

        /// @brief Reports whether an export is running.
        bool running() const noexcept;

        /// @brief Reports how far the running export is, 0 to 1.
        float progress() const noexcept;

        /// @brief Collects a finished export's outcome, once.
        /// @param error_out Receives the failure; empty when the file was written.
        /// @return True on the call that finds an export finished since the last call.
        bool take_result(std::string& error_out);

        /// @brief Renders a request on the calling thread. start() runs this on its own thread.
        /// @param progress Receives 0 to 1 as the render goes; may be null.
        /// @return The failure, or an empty string when the file was written.
        static std::string render(const BounceRequest& request, std::atomic<float>* progress);

    private:
        /// @brief Runs the export.
        std::thread worker_;

        /// @brief Records that the worker has not finished.
        std::atomic<bool> running_{false};

        /// @brief Records that a finished export's outcome has not been collected.
        std::atomic<bool> finished_{false};

        /// @brief Stores how far the export is.
        std::atomic<float> progress_{0.0f};

        /// @brief Stores the finished export's failure; read after finished_ is set.
        std::string error_;
};
