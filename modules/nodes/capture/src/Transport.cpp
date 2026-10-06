/**************************************************************************/
/* Transport.cpp                                                          */
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
 * @file Transport.cpp
 * @brief Implements the transport's position, loop range and playback speed.
 * @author Mustafa Garip
 */

#include "SushiDSP/capture/Transport.hpp"

#include <algorithm>
#include <cmath>

namespace SushiDSP
{
    namespace
    {
        constexpr std::uint64_t kLoopFieldLimit = 0x1'0000'0000ull;
        constexpr std::uint64_t kSpeedFractionMask = 0xFFFF'FFFFull;
    }

    void Transport::unpack_loop(std::uint64_t packed, std::uint64_t& out_begin,
                                std::uint64_t& out_length) noexcept
    {
        out_begin = packed >> 32;
        out_length = packed & 0xFFFF'FFFFull;
    }

    void Transport::advance(std::uint32_t frames) noexcept
    {
        const std::uint64_t position = position_.load(std::memory_order_relaxed);
        const std::uint64_t total = static_cast<std::uint64_t>(frames)
            * speed_q32_.load(std::memory_order_relaxed) + speed_remainder_;
        speed_remainder_ = total & kSpeedFractionMask;
        std::uint64_t next = position + (total >> 32);

        std::uint64_t begin = 0;
        std::uint64_t length = 0;
        unpack_loop(loop_.load(std::memory_order_acquire), begin, length);

        if (length != 0 && next >= begin + length && next >= begin)
        {
            next = begin + (next - begin) % length;
        }

        position_.store(next, std::memory_order_release);
    }

    void Transport::play() noexcept
    {
        state_.store(TransportState::Playing, std::memory_order_release);
    }

    void Transport::stop() noexcept
    {
        state_.store(TransportState::Stopped, std::memory_order_release);
    }

    void Transport::record() noexcept
    {
        state_.store(TransportState::Recording, std::memory_order_release);
    }

    void Transport::seek(std::uint64_t frames) noexcept
    {
        position_.store(frames, std::memory_order_release);
        speed_remainder_ = 0;
    }

    void Transport::set_loop(std::uint64_t begin_frames, std::uint64_t end_frames) noexcept
    {
        if (end_frames <= begin_frames || end_frames - begin_frames >= kLoopFieldLimit
            || begin_frames >= kLoopFieldLimit)
        {
            clear_loop();
            return;
        }

        const std::uint64_t length = end_frames - begin_frames;
        loop_.store((begin_frames << 32) | length, std::memory_order_release);
    }

    void Transport::clear_loop() noexcept
    {
        loop_.store(0, std::memory_order_release);
    }

    std::uint64_t Transport::position_frames() const noexcept
    {
        return position_.load(std::memory_order_acquire);
    }

    TransportState Transport::state() const noexcept
    {
        return state_.load(std::memory_order_acquire);
    }

    bool Transport::has_loop() const noexcept
    {
        return (loop_.load(std::memory_order_acquire) & 0xFFFF'FFFFull) != 0;
    }

    std::uint64_t Transport::loop_begin_frames() const noexcept
    {
        std::uint64_t begin = 0;
        std::uint64_t length = 0;
        unpack_loop(loop_.load(std::memory_order_acquire), begin, length);
        return length != 0 ? begin : 0;
    }

    std::uint64_t Transport::loop_end_frames() const noexcept
    {
        std::uint64_t begin = 0;
        std::uint64_t length = 0;
        unpack_loop(loop_.load(std::memory_order_acquire), begin, length);
        return length != 0 ? begin + length : 0;
    }

    bool Transport::loop_snapshot(std::uint64_t& out_begin, std::uint64_t& out_length) const noexcept
    {
        unpack_loop(loop_.load(std::memory_order_acquire), out_begin, out_length);
        if (out_length == 0)
        {
            out_begin = 0;
            return false;
        }
        return true;
    }

    void Transport::set_speed(double speed) noexcept
    {
        const double clamped = std::clamp(speed, kMinSpeed, kMaxSpeed);
        const auto   q32 = static_cast<std::uint64_t>(std::llround(clamped * static_cast<double>(1ull << 32)));
        speed_q32_.store(q32, std::memory_order_relaxed);
    }

    std::uint64_t Transport::speed_q32() const noexcept
    {
        return speed_q32_.load(std::memory_order_relaxed);
    }

    double Transport::speed() const noexcept
    {
        return static_cast<double>(speed_q32_.load(std::memory_order_relaxed))
            / static_cast<double>(1ull << 32);
    }
}
