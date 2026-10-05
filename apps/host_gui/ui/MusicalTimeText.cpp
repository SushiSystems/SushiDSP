/****************************************************************/
/* MusicalTimeText.cpp                                          */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file MusicalTimeText.cpp
 * @brief Implements the bar and beat text a timeline frame is named by.
 * @author Mustafa Garip
 */

#include "ui/MusicalTimeText.hpp"

#include <array>
#include <cstdio>

namespace ui
{
    std::string bar_beat_text(std::uint64_t frame, const SushiDSP::MusicalClock& clock)
    {
        const SushiDSP::BarBeat placed = clock.position_of(frame);
        std::array<char, 48>    text{};

        std::snprintf(text.data(), text.size(), "%llu.%u",
                      static_cast<unsigned long long>(placed.bar),
                      static_cast<unsigned>(placed.beat));
        return std::string(text.data());
    }

    std::string bar_beat_sixteenth_text(std::uint64_t frame, const SushiDSP::MusicalClock& clock)
    {
        const SushiDSP::BarBeat placed = clock.position_of(frame);
        const double            fraction = placed.fraction < 0.0 ? 0.0 : placed.fraction;
        std::uint32_t           sixteenth =
            static_cast<std::uint32_t>(fraction * static_cast<double>(kSixteenthsPerBeat)) + 1u;

        if (sixteenth > kSixteenthsPerBeat)
        {
            sixteenth = kSixteenthsPerBeat;
        }

        std::array<char, 64> text{};
        std::snprintf(text.data(), text.size(), "%llu · %u · %u",
                      static_cast<unsigned long long>(placed.bar),
                      static_cast<unsigned>(placed.beat), static_cast<unsigned>(sixteenth));
        return std::string(text.data());
    }
}
