/****************************************************************/
/* TakeLibrary.cpp                                              */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file TakeLibrary.cpp
 * @brief Implements the ownership and reference counts of loaded takes.
 * @author Mustafa Garip
 */

#include <algorithm>

#include "SushiDSP/capture/TakeLibrary.hpp"

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
        entries_.emplace_back(std::move(take), std::size_t{ 0 });
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
        entries_.erase(
            std::remove_if(entries_.begin(), entries_.end(),
                [](const Entry& entry) { return entry.second == 0; }),
            entries_.end());
    }

    std::size_t TakeLibrary::size() const noexcept
    {
        return entries_.size();
    }

    std::size_t TakeLibrary::reference_count(const Take* take) const noexcept
    {
        const auto found = std::find_if(entries_.begin(), entries_.end(),
            [take](const Entry& entry) { return entry.first.get() == take; });
        return found == entries_.end() ? std::size_t{ 0 } : found->second;
    }

    TakeLibrary::Entry* TakeLibrary::find(const Take* take) noexcept
    {
        if (take == nullptr)
        {
            return nullptr;
        }

        const auto found = std::find_if(entries_.begin(), entries_.end(),
            [take](const Entry& entry) { return entry.first.get() == take; });
        return found == entries_.end() ? nullptr : &*found;
    }
}
