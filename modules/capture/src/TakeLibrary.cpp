/**************************************************************************/
/* TakeLibrary.cpp                                                        */
/**************************************************************************/
/*                         This file is part of:                          */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                        https://sushisystems.io                         */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License");        */
/* you may not use this file except in compliance with the License.       */
/* You may obtain a copy of the License at                                */
/*                                                                        */
/*     http://www.apache.org/licenses/LICENSE-2.0                         */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

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

        // The Take stays where it was heap-allocated: entries_ growing moves the unique_ptr, never
        // the object it owns, so a pointer handed out here stays good until collect() frees it.
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
