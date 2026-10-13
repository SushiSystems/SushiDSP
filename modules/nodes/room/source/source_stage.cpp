/**************************************************************************/
/* source_stage.cpp                                                       */
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
 * @file source_stage.cpp
 * @brief Implements the per-cabinet stage: the split, the tail's network and the modal bank.
 * @author Mustafa Garip & Sushi Systems
 */

#include "SushiDSP/room/source_stage.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "SushiDSP/room/stage_limits.hpp"

namespace SushiDSP::Room
{
    namespace
    {
        /// @brief Counts the gains one modal row holds.
        constexpr std::size_t K_MODES = room::ModalDesign::K_MAX_MODES;

        /// @brief Holds the frames a tail ring keeps past the longest delay for the read kernel.
        constexpr std::uint32_t K_TAIL_RING_MARGIN = 4;
    } // namespace

    bool SourceStage::same_resonators(const room::ModalDesign& retiring,
                                      const room::ModalDesign& arriving) noexcept
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

    void SourceStage::prepare(double sample_rate, std::uint32_t alignment_frames,
                              std::uint32_t max_block_frames)
    {
        sample_rate_ = sample_rate;
        max_block_ = max_block_frames;
        const std::uint32_t max_delay = max_early_delay_frames(sample_rate);
        low_.assign(max_block_frames, 0.0f);
        high_.assign(max_block_frames, 0.0f);
        ring_.prepare(alignment_frames + max_delay + K_ITD_MARGIN_FRAMES, max_block_frames);
        network_.prepare(static_cast<std::uint32_t>(
            std::ceil(room::FdnDesign::K_MAX_LINE_SECONDS * sample_rate)));
        for (std::uint32_t sum = 0; sum < 2; ++sum)
        {
            tail_sums_[sum].assign(max_block_frames, 0.0f);
            tail_rings_[sum].prepare(max_delay + K_TAIL_RING_MARGIN, max_block_frames);
        }
        for (room::ModalBank& bank : banks_)
        {
            bank.prepare(max_block_frames);
        }
        for (ModalSet* modal : {&bank_modal_[0], &bank_modal_[1], &from_, &blend_})
        {
            modal->gains.assign(static_cast<std::size_t>(K_ROWS) * K_MODES, 0.0f);
            modal->rows = 0;
            modal->design.count = 0;
        }
        modal_.assign(static_cast<std::size_t>(K_ROWS) * max_block_frames, 0.0f);
        retiring_modal_.assign(static_cast<std::size_t>(K_ROWS) * max_block_frames, 0.0f);
        reset();
    }

    void SourceStage::reset() noexcept
    {
        crossover_.reset();
        ring_.reset();
        network_.reset();
        for (room::DelayRing& ring : tail_rings_)
        {
            ring.reset();
        }
        for (room::ModalBank& bank : banks_)
        {
            bank.reset();
        }
        std::fill(modal_.begin(), modal_.end(), 0.0f);
        live_ = false;
        pending_ = false;
        fading_ = false;
        ramping_ = false;
    }

    bool SourceStage::live() const noexcept
    {
        return live_;
    }

    void SourceStage::load_modal(const EnvironmentRenderSet& set, std::uint32_t source,
                                 ModalSet& into) noexcept
    {
        const room::ModalDesign& ears = set.pairs[source][0].modal;
        into.design = ears;
        into.rows = std::min(2u + set.microphone_count, K_ROWS);
        float* const gains = into.gains.data();
        for (std::size_t mode = 0; mode < K_MODES; ++mode)
        {
            const bool in_use = mode < ears.count;
            gains[mode] = in_use ? ears.modes[mode].ear_gain[0] : 0.0f;
            gains[K_MODES + mode] = in_use ? ears.modes[mode].ear_gain[1] : 0.0f;
        }
        for (std::uint32_t row = 2; row < into.rows; ++row)
        {
            const room::ModalDesign& microphone = set.pairs[source][row - 1u].modal;
            float* const row_gains = gains + static_cast<std::size_t>(row) * K_MODES;
            for (std::size_t mode = 0; mode < K_MODES; ++mode)
            {
                row_gains[mode] = mode < ears.count && mode < microphone.count
                                      ? microphone.modes[mode].ear_gain[0]
                                      : 0.0f;
            }
        }
    }

    bool SourceStage::gains_moved(const ModalSet& retiring, const ModalSet& arriving) noexcept
    {
        if (retiring.rows != arriving.rows)
        {
            return true;
        }
        for (std::uint32_t index = 0; index < arriving.design.count; ++index)
        {
            if (retiring.design.modes[index].source_gain !=
                arriving.design.modes[index].source_gain)
            {
                return true;
            }
        }
        const std::size_t count = static_cast<std::size_t>(arriving.rows) * K_MODES;
        return !std::equal(arriving.gains.begin(),
                           arriving.gains.begin() + static_cast<std::ptrdiff_t>(count),
                           retiring.gains.begin());
    }

    void SourceStage::adopt(const EnvironmentRenderSet& set, std::uint32_t source) noexcept
    {
        const room::RoomRenderSet& ears = set.pairs[source][0];
        crossover_.set_crossover(sample_rate_, ears.crossover_hz);

        // The network's delay and level belong to each receiver; the lines belong to the room.
        pending_design_ = ears.late;
        pending_design_.delay_to_crossover_frames = 0.0f;
        pending_ = true;

        if (!live_)
        {
            network_design_ = pending_design_;
            pending_ = false;
            load_modal(set, source, bank_modal_[active_bank_]);
            banks_[active_bank_].reset();
            fading_ = false;
            ramping_ = false;
            live_ = true;
            return;
        }

        // A running ramp's last step is the set the active bank rendered last.
        ModalSet& active = bank_modal_[active_bank_];
        if (ramping_)
        {
            active.design = blend_.design;
            active.gains = blend_.gains;
            active.rows = blend_.rows;
        }
        const std::uint32_t arriving = active_bank_ == 0 ? 1u : 0u;
        ModalSet& spare = bank_modal_[arriving];
        load_modal(set, source, spare);
        if (same_resonators(active.design, spare.design))
        {
            // Same resonators keep their state and ramp their gains alone.
            fading_ = false;
            ramping_ = gains_moved(active, spare);
            from_.design = active.design;
            from_.gains = active.gains;
            from_.rows = active.rows;
            blend_.design = spare.design;
            blend_.gains = spare.gains;
            blend_.rows = spare.rows;
            active.design = spare.design;
            active.gains = spare.gains;
            active.rows = spare.rows;
            return;
        }
        banks_[arriving].reset();
        active_bank_ = arriving;
        fading_ = true;
        ramping_ = false;
    }

    void SourceStage::apply_pending_when_due(const room::FadeWindow& window) noexcept
    {
        if (!pending_ || window.done < window.lag)
        {
            return;
        }
        network_design_ = pending_design_;
        pending_ = false;
    }

    void SourceStage::write_input(const float* input, std::uint32_t frames) noexcept
    {
        float* const __restrict low = low_.data();
        float* const __restrict high = high_.data();
        if (input == nullptr)
        {
            for (std::uint32_t frame = 0; frame < frames; ++frame)
            {
                crossover_.process_sample(0.0f, low[frame], high[frame]);
            }
        }
        else
        {
            for (std::uint32_t frame = 0; frame < frames; ++frame)
            {
                crossover_.process_sample(input[frame], low[frame], high[frame]);
            }
        }
        ring_.write(high, frames);
    }

    void SourceStage::render_tail(std::uint32_t frames) noexcept
    {
        network_.process_sums(network_design_, high_.data(), tail_sums_[0].data(),
                              tail_sums_[1].data(), frames);
        tail_rings_[0].write(tail_sums_[0].data(), frames);
        tail_rings_[1].write(tail_sums_[1].data(), frames);
    }

    void SourceStage::run_bank(std::uint32_t bank, const ModalSet& modal, float* outputs,
                               std::uint32_t offset, std::uint32_t frames) noexcept
    {
        std::array<room::ModalBank::ReadOut, K_ROWS> read_outs{};
        for (std::uint32_t row = 0; row < modal.rows; ++row)
        {
            read_outs[row].gains = modal.gains.data() + static_cast<std::size_t>(row) * K_MODES;
            read_outs[row].output = outputs + static_cast<std::size_t>(row) * max_block_ + offset;
        }
        banks_[bank].process(modal.design, low_.data() + offset, read_outs.data(), modal.rows,
                             frames);
    }

    void SourceStage::render_modal(std::uint32_t frames, const room::FadeWindow& window) noexcept
    {
        for (std::uint32_t row = 0; row < K_ROWS; ++row)
        {
            std::fill_n(modal_.data() + static_cast<std::size_t>(row) * max_block_, frames, 0.0f);
        }
        const ModalSet& active = bank_modal_[active_bank_];

        if (fading_ && window.fading > 0)
        {
            const std::uint32_t retiring = active_bank_ == 0 ? 1u : 0u;
            const ModalSet& old = bank_modal_[retiring];
            for (std::uint32_t row = 0; row < K_ROWS; ++row)
            {
                std::fill_n(retiring_modal_.data() + static_cast<std::size_t>(row) * max_block_,
                            window.fading, 0.0f);
            }
            run_bank(active_bank_, active, modal_.data(), 0u, frames);
            run_bank(retiring, old, retiring_modal_.data(), 0u, window.fading);
            for (std::uint32_t row = 0; row < K_ROWS; ++row)
            {
                float* const __restrict out =
                    modal_.data() + static_cast<std::size_t>(row) * max_block_;
                const float* const __restrict before =
                    retiring_modal_.data() + static_cast<std::size_t>(row) * max_block_;
                for (std::uint32_t frame = 0; frame < window.fading; ++frame)
                {
                    const float weight = room::window_weight(window, frame, false);
                    out[frame] = before[frame] + weight * (out[frame] - before[frame]);
                }
            }
            return;
        }
        if (ramping_ && window.fading > 0)
        {
            for (std::uint32_t frame = 0; frame < window.fading; ++frame)
            {
                render_ramp(frame, room::window_weight(window, frame, false));
            }
            run_bank(active_bank_, active, modal_.data(), window.fading, frames - window.fading);
            return;
        }
        run_bank(active_bank_, active, modal_.data(), 0u, frames);
    }

    void SourceStage::render_ramp(std::uint32_t frame, float weight) noexcept
    {
        const ModalSet& to = bank_modal_[active_bank_];
        for (std::uint32_t index = 0; index < to.design.count; ++index)
        {
            const float from = from_.design.modes[index].source_gain;
            const float target = to.design.modes[index].source_gain;
            blend_.design.modes[index].source_gain = from + weight * (target - from);
        }
        // A row the retiring set did not hold ramps up from silence.
        const std::size_t count = static_cast<std::size_t>(to.rows) * K_MODES;
        const std::size_t held = static_cast<std::size_t>(from_.rows) * K_MODES;
        const float* const __restrict from_gains = from_.gains.data();
        const float* const __restrict to_gains = to.gains.data();
        float* const __restrict blend_gains = blend_.gains.data();
        for (std::size_t index = 0; index < count; ++index)
        {
            const float from = index < held ? from_gains[index] : 0.0f;
            blend_gains[index] = from + weight * (to_gains[index] - from);
        }
        blend_.rows = to.rows;
        run_bank(active_bank_, blend_, modal_.data(), frame, 1u);
    }

    void SourceStage::settle() noexcept
    {
        fading_ = false;
        ramping_ = false;
    }

    const float* SourceStage::high() const noexcept
    {
        return high_.data();
    }

    const room::DelayRing& SourceStage::ring() const noexcept
    {
        return ring_;
    }

    const room::DelayRing& SourceStage::tail_ring(std::uint32_t sum) const noexcept
    {
        return tail_rings_[sum == 0 ? 0 : 1];
    }

    const float* SourceStage::modal(std::uint32_t row) const noexcept
    {
        return modal_.data() + static_cast<std::size_t>(std::min(row, K_ROWS - 1u)) * max_block_;
    }
} // namespace SushiDSP::Room
