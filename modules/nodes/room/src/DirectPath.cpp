/****************************************************************/
/* DirectPath.cpp                                               */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file DirectPath.cpp
 * @brief The read, the two sections and the convolution one ear's direct sound runs through.
 * @author Mustafa Garip & Sushi Systems
 */

#include "SushiDSP/room/DirectPath.hpp"

#include <algorithm>

namespace SushiDSP::room
{
    namespace
    {
        /// @brief Runs a tap's two sections over a block in place, in the early renderer's
        /// arithmetic: double state, the first section's output fed to the second unrounded.
        void filter_sections(const EarlyTap& tap, std::array<double, 8>& state, float* samples,
                             std::uint32_t frames) noexcept
        {
            const double b0a = tap.section_a[0];
            const double b1a = tap.section_a[1];
            const double b2a = tap.section_a[2];
            const double a1a = tap.section_a[3];
            const double a2a = tap.section_a[4];
            const double b0b = tap.section_b[0];
            const double b1b = tap.section_b[1];
            const double b2b = tap.section_b[2];
            const double a1b = tap.section_b[3];
            const double a2b = tap.section_b[4];
            double xa1 = state[0];
            double xa2 = state[1];
            double ya1 = state[2];
            double ya2 = state[3];
            double xb1 = state[4];
            double xb2 = state[5];
            double yb1 = state[6];
            double yb2 = state[7];
            for (std::uint32_t frame = 0; frame < frames; ++frame)
            {
                const double x = static_cast<double>(samples[frame]);
                const double ya = b0a * x + b1a * xa1 + b2a * xa2 - a1a * ya1 - a2a * ya2;
                xa2 = xa1;
                xa1 = x;
                ya2 = ya1;
                ya1 = ya;
                const double yb = b0b * ya + b1b * xb1 + b2b * xb2 - a1b * yb1 - a2b * yb2;
                xb2 = xb1;
                xb1 = ya;
                yb2 = yb1;
                yb1 = yb;
                samples[frame] = static_cast<float>(yb);
            }
            state = { xa1, xa2, ya1, ya2, xb1, xb2, yb1, yb2 };
        }
    }

    void DirectPath::prepare(std::uint32_t alignment_frames, std::uint32_t max_block_frames)
    {
        alignment_frames_ = alignment_frames;
        scratch_.assign(max_block_frames, 0.0f);
        for (std::uint32_t bank = 0; bank < kBanks; ++bank)
        {
            for (Ear& ear : banks_[bank].ears)
            {
                ear.hrir.prepare(max_block_frames);
            }
            clear_bank(bank);
        }
    }

    void DirectPath::reset() noexcept
    {
        for (std::uint32_t bank = 0; bank < kBanks; ++bank)
        {
            clear_bank(bank);
        }
    }

    void DirectPath::load_bank(std::uint32_t bank, const EarlyTap& tap,
                               const RoomRenderSet& set) noexcept
    {
        Bank& target = banks_[bank];
        target.tap = tap;
        target.active = true;
        target.ears[0].hrir.load(set.direct_left.data());
        target.ears[1].hrir.load(set.direct_right.data());
        // A positive ITD puts the right ear later; the near ear reads at the tap's own delay.
        const double itd = static_cast<double>(set.direct_itd_frames);
        target.ears[0].extra_delay = std::max(-itd, 0.0);
        target.ears[1].extra_delay = std::max(itd, 0.0);
    }

    void DirectPath::clear_bank(std::uint32_t bank) noexcept
    {
        Bank& target = banks_[bank];
        target.active = false;
        for (Ear& ear : target.ears)
        {
            ear.filter_state.fill(0.0);
            ear.hrir.reset();
        }
    }

    void DirectPath::carry_bank(std::uint32_t from, std::uint32_t to) noexcept
    {
        if (from == to)
        {
            return;
        }
        for (std::uint32_t ear = 0; ear < kEars; ++ear)
        {
            banks_[to].ears[ear].filter_state = banks_[from].ears[ear].filter_state;
            banks_[to].ears[ear].hrir.carry_from(banks_[from].ears[ear].hrir);
        }
    }

    const EarlyTap& DirectPath::tap(std::uint32_t bank) const noexcept
    {
        return banks_[bank].tap;
    }

    bool DirectPath::active(std::uint32_t bank) const noexcept
    {
        return banks_[bank].active;
    }

    void DirectPath::render(const DelayRing& ring, std::uint32_t bank, float* left, float* right,
                            std::uint32_t frames) noexcept
    {
        Bank& source = banks_[bank];
        if (!source.active)
        {
            std::fill_n(left, frames, 0.0f);
            std::fill_n(right, frames, 0.0f);
            return;
        }
        render_ear(ring, source, 0, left, frames);
        render_ear(ring, source, 1, right, frames);
    }

    void DirectPath::render_ear(const DelayRing& ring, Bank& bank, std::uint32_t ear,
                                float* output, std::uint32_t frames) noexcept
    {
        Ear& chain = bank.ears[ear];
        float* const scratch = scratch_.data();
        ring.read(static_cast<double>(alignment_frames_) + bank.tap.delay_frames
                      + chain.extra_delay,
                  bank.tap.gain, scratch, frames);
        filter_sections(bank.tap, chain.filter_state, scratch, frames);
        chain.hrir.process(scratch, output, frames);
    }
}
