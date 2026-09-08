/**************************************************************************/
/* TapTempo.cpp                                                           */
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

#include "ui/TapTempo.hpp"

#include <algorithm>

namespace ui
{
    std::optional<double> TapTempo::tap(double seconds)
    {
        if (count_ != 0 && seconds - taps_[count_ - 1u] > kMaximumGapSeconds)
        {
            count_ = 0;
        }

        if (count_ == kTapCount)
        {
            for (std::size_t index = 1; index < kTapCount; ++index)
            {
                taps_[index - 1u] = taps_[index];
            }
            --count_;
        }

        taps_[count_++] = seconds;

        if (count_ < 2u)
        {
            return std::nullopt;
        }

        const double span = taps_[count_ - 1u] - taps_[0];
        const double interval = span / static_cast<double>(count_ - 1u);

        if (!(interval > 0.0))
        {
            return std::nullopt;
        }

        return std::clamp(60.0 / interval, kMinimumTempo, kMaximumTempo);
    }

    void TapTempo::reset() noexcept
    {
        count_ = 0;
    }

    std::size_t TapTempo::tap_count() const noexcept
    {
        return count_;
    }
}
