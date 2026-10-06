/**************************************************************************/
/* TakeLibrary.hpp                                                        */
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
 * @file TakeLibrary.hpp
 * @brief Declares the owner of every loaded take.
 * @author Mustafa Garip
 */

#pragma once

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

#include "SushiDSP/capture/Take.hpp"

namespace SushiDSP
{
    /// @brief Owns every loaded Take and hands out the borrowed pointers clips hold. Control
    /// thread only: it carries no atomic and has no real-time entry point, so nothing the audio
    /// thread runs ever steps a count or frees a take. The lifetime argument is in
    /// docs/archive/superpowers/specs/2026-09-08-arrangement-view-design.md §5.
    class TakeLibrary
    {
    public:
        TakeLibrary() = default;
        ~TakeLibrary() = default;

        TakeLibrary(const TakeLibrary&) = delete;
        TakeLibrary& operator=(const TakeLibrary&) = delete;

        /// @brief Takes ownership of a loaded take and returns the borrowed pointer clips name it
        /// by, with a hold count of zero.
        /// @param take The loaded take. Must not be null.
        /// @return The borrowed pointer, stable for the take's whole life.
        /// @pre Called on the control thread.
        const Take* insert(std::unique_ptr<const Take> take);

        /// @brief Records that one more live timeline names this take.
        /// @param take A pointer a previous insert() returned. Ignored when null or unknown.
        /// @pre Called on the control thread.
        void retain(const Take* take);

        /// @brief Records that one fewer live timeline names this take.
        /// @param take A pointer a previous insert() returned. Ignored when null or unknown.
        /// @pre The take's hold count is not already zero.
        /// @pre Called on the control thread.
        void release(const Take* take);

        /// @brief Destroys every take whose hold count is zero.
        /// @pre Every lane slot has been reclaimed since the last release(), so no retired
        /// timeline the audio thread can still read names a take this call frees.
        /// @pre Called on the control thread.
        void collect();

        /// @brief Returns how many takes the library currently owns.
        std::size_t size() const noexcept;

        /// @brief Returns how many live timelines name a take.
        /// @param take A pointer a previous insert() returned.
        /// @return The hold count, or zero when the take is unknown.
        std::size_t reference_count(const Take* take) const noexcept;

    private:
        /// @brief Pairs one owned take with the number of live timelines naming it.
        using Entry = std::pair<std::unique_ptr<const Take>, std::size_t>;

        /// @brief Finds the entry owning a take.
        /// @param take The borrowed pointer to look up.
        /// @return The entry, or nullptr when the take is unknown.
        Entry* find(const Take* take) noexcept;

        /// @brief Owns every take, each heap-allocated once and never moved.
        std::vector<Entry> entries_;
    };
}
