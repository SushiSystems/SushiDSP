/**************************************************************************/
/* musical_clock.cpp                                                      */
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
 * @file musical_clock.cpp
 * @brief Implements the conversions between sample frames and bars and beats.
 * @author Mustafa Garip
 */

#include "SushiDSP/capture/musical_clock.hpp"

#include <cmath>
#include <limits>

namespace SushiDSP
{
    namespace
    {
        constexpr double K_FIXED_POINT_SCALE = 4294967296.0;
        constexpr double K_QUARTER_NOTE_UNIT = 4.0;

        /// @brief Returns floor(value * 2^32 / divisor) and leaves the exact remainder of that
        /// division behind, without needing a 128-bit intermediate.
        /// @param value The dividend before it is scaled.
        /// @param divisor The 32.32 divisor.
        /// @param out_remainder Receives (value * 2^32) modulo divisor.
        /// @pre divisor is not zero.
        std::uint64_t divide_scaled(std::uint64_t value, std::uint64_t divisor,
                                    std::uint64_t& out_remainder) noexcept
        {
            std::uint64_t whole = value / divisor;
            std::uint64_t remainder = value % divisor;
            std::uint64_t fraction = 0;

            for (int bit = 0; bit < 32; ++bit)
            {
                const bool carry = (remainder >> 63) != 0;
                remainder <<= 1;
                fraction <<= 1;

                if (carry || remainder >= divisor)
                {
                    remainder -= divisor;
                    fraction |= 1ull;
                }
            }

            out_remainder = remainder;

            if (whole > (std::numeric_limits<std::uint64_t>::max() >> 32))
            {
                return std::numeric_limits<std::uint64_t>::max();
            }

            return (whole << 32) | fraction;
        }
    } // namespace

    MusicalClock::MusicalClock() noexcept
        : beat_step_q32_(0), beats_per_minute_(120.0), sample_rate_(48000.0), beats_per_bar_(4),
          beat_unit_(4)
    {
        republish_step();
    }

    void MusicalClock::set_tempo(double beats_per_minute) noexcept
    {
        if (!std::isfinite(beats_per_minute) || beats_per_minute <= 0.0)
        {
            return;
        }

        beats_per_minute_ = beats_per_minute;
        republish_step();
    }

    void MusicalClock::set_meter(std::uint32_t beats_per_bar, std::uint32_t beat_unit) noexcept
    {
        if (beats_per_bar == 0 || beat_unit == 0)
        {
            return;
        }

        beats_per_bar_ = beats_per_bar;
        beat_unit_ = beat_unit;
        republish_step();
    }

    void MusicalClock::set_sample_rate(double sample_rate) noexcept
    {
        if (!std::isfinite(sample_rate) || sample_rate <= 0.0)
        {
            return;
        }

        sample_rate_ = sample_rate;
        republish_step();
    }

    void MusicalClock::republish_step() noexcept
    {
        const double frames_per_quarter = sample_rate_ * 60.0 / beats_per_minute_;
        const double frames_per_beat =
            frames_per_quarter * K_QUARTER_NOTE_UNIT / static_cast<double>(beat_unit_);
        const double scaled = frames_per_beat * K_FIXED_POINT_SCALE;

        std::uint64_t step = 0;

        if (std::isfinite(scaled) && scaled >= 1.0)
        {
            const double limit = static_cast<double>(std::numeric_limits<std::uint64_t>::max());
            step = scaled >= limit ? std::numeric_limits<std::uint64_t>::max()
                                   : static_cast<std::uint64_t>(std::llround(scaled));
        }

        beat_step_q32_.store(step, std::memory_order_release);
    }

    std::uint64_t MusicalClock::beat_step_q32() const noexcept
    {
        return beat_step_q32_.load(std::memory_order_relaxed);
    }

    std::uint32_t MusicalClock::beats_per_bar() const noexcept
    {
        return beats_per_bar_;
    }

    double MusicalClock::beats_per_minute() const noexcept
    {
        return beats_per_minute_;
    }

    std::uint32_t MusicalClock::beat_unit() const noexcept
    {
        return beat_unit_;
    }

    double MusicalClock::sample_rate() const noexcept
    {
        return sample_rate_;
    }

    BarBeat MusicalClock::position_of(std::uint64_t frame) const noexcept
    {
        const std::uint64_t step = beat_step_q32_.load(std::memory_order_acquire);

        if (step == 0)
        {
            return BarBeat{1u, 1u, 0.0};
        }

        std::uint64_t remainder = 0;
        const std::uint64_t beat = divide_scaled(frame, step, remainder);
        const std::uint64_t per_bar = beats_per_bar_;

        BarBeat placed;
        placed.bar = beat / per_bar + 1u;
        placed.beat = static_cast<std::uint32_t>(beat % per_bar) + 1u;
        placed.fraction = static_cast<double>(remainder) / static_cast<double>(step);
        return placed;
    }

    std::uint64_t MusicalClock::frame_of(std::uint64_t bar, std::uint32_t beat) const noexcept
    {
        const std::uint64_t step = beat_step_q32_.load(std::memory_order_acquire);

        if (step == 0)
        {
            return 0;
        }

        const std::uint64_t bar_index = bar > 0 ? bar - 1u : 0;
        const std::uint64_t beat_index = beat > 0 ? beat - 1u : 0;
        const std::uint64_t absolute = bar_index * beats_per_bar_ + beat_index;

        if (absolute > std::numeric_limits<std::uint64_t>::max() / step)
        {
            return std::numeric_limits<std::uint64_t>::max() >> 32;
        }

        return (absolute * step) >> 32;
    }
} // namespace SushiDSP
