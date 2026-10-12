/**************************************************************************/
/* modal_path.cpp                                                         */
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
 * @file modal_path.cpp
 * @brief The match rule, the gain ramp and the bank blend the standing waves swap through.
 * @author Mustafa Garip & Sushi Systems
 */

#include "SushiDSP/room/modal_path.hpp"

#include <algorithm>

namespace SushiDSP::room
{
    namespace
    {
        /// @brief Returns true when two designs give any paired mode a different source or ear
        /// gain.
        bool gains_moved(const ModalDesign& retiring, const ModalDesign& arriving) noexcept
        {
            for (std::uint32_t index = 0; index < arriving.count; ++index)
            {
                const ModalDesign::Mode& before = retiring.modes[index];
                const ModalDesign::Mode& after = arriving.modes[index];
                if (before.source_gain != after.source_gain || before.ear_gain != after.ear_gain)
                {
                    return true;
                }
            }
            return false;
        }
    } // namespace

    bool ModalPath::same_resonators(const ModalDesign& retiring,
                                    const ModalDesign& arriving) noexcept
    {
        if (retiring.count != arriving.count)
        {
            return false;
        }
        for (std::uint32_t index = 0; index < arriving.count; ++index)
        {
            if (retiring.modes[index].section != arriving.modes[index].section)
            {
                return false;
            }
        }
        return true;
    }

    void ModalPath::prepare(std::uint32_t max_block_frames)
    {
        for (ModalBank& bank : banks_)
        {
            bank.prepare(max_block_frames);
        }
        low_.assign(max_block_frames, 0.0f);
        left_.assign(max_block_frames, 0.0f);
        right_.assign(max_block_frames, 0.0f);
        retiring_left_.assign(max_block_frames, 0.0f);
        retiring_right_.assign(max_block_frames, 0.0f);
    }

    void ModalPath::reset() noexcept
    {
        for (ModalBank& bank : banks_)
        {
            bank.reset();
        }
        fading_ = false;
        ramping_ = false;
    }

    float* ModalPath::low_buffer() noexcept
    {
        return low_.data();
    }

    const float* ModalPath::field(std::uint32_t ear) const noexcept
    {
        return ear == 0 ? left_.data() : right_.data();
    }

    void ModalPath::settle() noexcept
    {
        fading_ = false;
        ramping_ = false;
    }

    void ModalPath::adopt_first(const ModalDesign& design) noexcept
    {
        bank_modes_[active_bank_] = design;
        banks_[active_bank_].reset();
    }

    void ModalPath::adopt(const ModalDesign& design) noexcept
    {
        // A running ramp's last step is the design the active bank rendered last.
        ModalDesign& active = bank_modes_[active_bank_];
        if (ramping_)
        {
            active = blend_;
        }
        if (same_resonators(active, design))
        {
            // Same resonators keep their state and ramp their gains alone (spec §9a.2).
            fading_ = false;
            ramping_ = gains_moved(active, design);
            from_ = active;
            blend_ = design;
            active = design;
            return;
        }
        const std::uint32_t arriving = active_bank_ == 0 ? 1u : 0u;
        banks_[arriving].reset();
        bank_modes_[arriving] = design;
        active_bank_ = arriving;
        fading_ = true;
        ramping_ = false;
    }

    void ModalPath::render(std::uint32_t frames, const FadeWindow& window,
                           bool microphone_involved) noexcept
    {
        const float* const low = low_.data();
        float* const left = left_.data();
        float* const right = right_.data();
        std::fill_n(left, frames, 0.0f);
        std::fill_n(right, frames, 0.0f);

        if (fading_ && window.fading > 0)
        {
            const std::uint32_t retiring = active_bank_ == 0 ? 1u : 0u;
            float* const old_left = retiring_left_.data();
            float* const old_right = retiring_right_.data();
            std::fill_n(old_left, window.fading, 0.0f);
            std::fill_n(old_right, window.fading, 0.0f);
            banks_[active_bank_].process(bank_modes_[active_bank_], low, left, right, frames);
            banks_[retiring].process(bank_modes_[retiring], low, old_left, old_right,
                                     window.fading);
            blend_pair(left, right, old_left, old_right, window, microphone_involved);
            return;
        }
        if (ramping_ && window.fading > 0)
        {
            for (std::uint32_t frame = 0; frame < window.fading; ++frame)
            {
                render_ramp(frame, window_weight(window, frame, microphone_involved));
            }
            banks_[active_bank_].process(bank_modes_[active_bank_], low + window.fading,
                                         left + window.fading, right + window.fading,
                                         frames - window.fading);
            return;
        }
        banks_[active_bank_].process(bank_modes_[active_bank_], low, left, right, frames);
    }

    void ModalPath::render_ramp(std::uint32_t frame, float weight) noexcept
    {
        const ModalDesign& to = bank_modes_[active_bank_];
        for (std::uint32_t index = 0; index < to.count; ++index)
        {
            const ModalDesign::Mode& from = from_.modes[index];
            const ModalDesign::Mode& target = to.modes[index];
            ModalDesign::Mode& blend = blend_.modes[index];
            blend.source_gain = from.source_gain + weight * (target.source_gain - from.source_gain);
            for (std::size_t ear = 0; ear < K_EARS; ++ear)
            {
                blend.ear_gain[ear] =
                    from.ear_gain[ear] + weight * (target.ear_gain[ear] - from.ear_gain[ear]);
            }
        }
        banks_[active_bank_].process(blend_, low_.data() + frame, left_.data() + frame,
                                     right_.data() + frame, 1u);
    }
} // namespace SushiDSP::room
