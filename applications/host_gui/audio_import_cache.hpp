/**************************************************************************/
/* audio_import_cache.hpp                                                 */
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
 * @file audio_import_cache.hpp
 * @brief Declares the folder of decoded WAVs the host keeps beside its executable.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <string>

namespace SushiDSP::host
{
    /// @brief Turns a file the tree cannot read into one it can: decodes an mp3, converts it to
    /// the rig's rate and writes a WAV into a folder of its own, handing back that WAV's path.
    /// A session stores a clip as a path, so the decode has to leave a file behind. Control
    /// thread only: it decodes, allocates, writes the filesystem and throws.
    class AudioImportCache
    {
        public:
            /// @brief Names the folder the decoded WAVs are written into.
            /// @param directory The folder; it is created on the first import.
            explicit AudioImportCache(std::string directory);

            /// @brief Reports whether a path names a file this cache can turn into a WAV.
            /// @param path The file's path; its extension is read case-insensitively.
            /// @return True for an mp3, false for everything else, a WAV included.
            static bool can_import(const std::string& path);

            /// @brief Names the WAV a source file converts into, without touching it. The name
            /// carries the source's stem, a hash of its path, size and last write time, and the
            /// target rate, so an edited source and a rig at another rate each get their own entry.
            /// @param source_path The file to convert.
            /// @param target_rate The rate the WAV is written at, in Hz.
            /// @return The file name, with no directory part.
            static std::string entry_name(const std::string& source_path, double target_rate);

            /// @brief Names the full path a source file converts into.
            /// @param source_path The file to convert.
            /// @param target_rate The rate the WAV is written at, in Hz.
            /// @return The path inside this cache's folder.
            std::string entry_path(const std::string& source_path, double target_rate) const;

            /// @brief Returns the WAV for a source file, decoding it only when the folder lacks it.
            /// @param source_path The file to convert.
            /// @param target_rate The rate to write at, in Hz; a rate that is not positive writes
            /// the source's own rate.
            /// @return The WAV's path.
            /// @throws std::runtime_error when the source cannot be decoded or the WAV cannot be
            /// written.
            /// @see applications/host_gui/README.md
            std::string import(const std::string& source_path, double target_rate) const;

            /// @brief Returns the folder the decoded WAVs are written into.
            const std::string& directory() const;

        private:
            /// @brief Holds the folder the decoded WAVs are written into.
            std::string directory_;
    };
} // namespace SushiDSP::host
