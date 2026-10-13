/**************************************************************************/
/* session_file.hpp                                                       */
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
 *  @file session_file.hpp
 *  @brief Declares the session's documents: the project, and one track's rig preset.
 *  @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <cstddef>
#include <string>

#include "session.hpp"

/// @brief Holds the extension a project file is written with.
inline constexpr const char* K_PROJECT_FILE_EXTENSION = ".sdproject";

/// @brief Writes the whole session as one document, which is what a project file holds: the first track's rig and the timeline as a
/// preset writes them, and every track's strip, preset path and rig under `"track_rigs"`.
/// @throws std::runtime_error when a rig does not serialize.
std::string session_to_json(const Session& session);

/// @brief Replaces the session from a document session_to_json() wrote, or from a preset of one
/// rig, whose lanes then become tracks that pass their audio by an empty rig.
/// @throws std::runtime_error when the document is malformed or a rig in it is refused.
void session_from_json(Session& session, const std::string& json_text);

/// @brief Empties the session into a new project: one track, no clip, an empty rig and a
/// default strip. The tempo and the loop are left as they are.
void new_project(Session& session);

/// @brief Loads a rig preset file into a track's rig and remembers the path. The track's lane is
/// left as it is.
/// @return False, with the reason, when the file cannot be read or the rig refuses it.
bool load_track_rig(Session& session, std::size_t index, const std::string& path,
                    std::string& error_out);

/// @brief Writes a track's rig to a preset file and remembers the path.
/// @return False, with the reason, when the rig does not serialize or the file cannot be written.
bool save_track_rig(Session& session, std::size_t index, const std::string& path,
                    std::string& error_out);

/// @brief Reads a whole text file.
/// @return False, with the reason, when the file cannot be opened.
bool read_text_file(const std::string& path, std::string& text_out, std::string& error_out);

/// @brief Writes a whole text file, replacing what is there.
/// @return False, with the reason, when the file cannot be opened for writing.
bool write_text_file(const std::string& path, const std::string& text, std::string& error_out);
