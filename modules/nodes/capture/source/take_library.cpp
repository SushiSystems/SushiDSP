/**************************************************************************/
/* take_library.cpp                                                       */
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
 * @file take_library.cpp
 * @brief Implements the ownership and reference counts of loaded takes.
 * @author Mustafa Garip
 */

#include <algorithm>

#include <SushiDSP/nodes/capture/take_library.hpp>

namespace SushiDSP
{
    const Take* TakeLibrary::insert(std::unique_ptr<const Take> take)
    {
        if (take == nullptr)
        {
            return nullptr;
        }

        // A heap-allocated Take never moves; see modules/nodes/capture/README.md.
        const Take* borrowed = take.get();
        entries_.emplace_back(std::move(take), std::size_t{0});
        return borrowed;
    }

    void TakeLibrary::retain(const Take* take)
    {
        if (Entry* entry = find(take))
        {
            ++entry->second;
        }
    }

    void TakeLibrary::release(const Take* take)
    {
        if (Entry* entry = find(take))
        {
            if (entry->second != 0)
            {
                --entry->second;
            }
        }
    }

    void TakeLibrary::collect()
    {
        entries_.erase(std::remove_if(entries_.begin(), entries_.end(),
                                      [](const Entry& entry)
                                      {
                                          return entry.second == 0;
                                      }),
                       entries_.end());
    }

    std::size_t TakeLibrary::size() const noexcept
    {
        return entries_.size();
    }

    std::size_t TakeLibrary::reference_count(const Take* take) const noexcept
    {
        const auto found = std::find_if(entries_.begin(), entries_.end(),
                                        [take](const Entry& entry)
                                        {
                                            return entry.first.get() == take;
                                        });
        return found == entries_.end() ? std::size_t{0} : found->second;
    }

    TakeLibrary::Entry* TakeLibrary::find(const Take* take) noexcept
    {
        if (take == nullptr)
        {
            return nullptr;
        }

        const auto found = std::find_if(entries_.begin(), entries_.end(),
                                        [take](const Entry& entry)
                                        {
                                            return entry.first.get() == take;
                                        });
        return found == entries_.end() ? nullptr : &*found;
    }
} // namespace SushiDSP
