/**************************************************************************/
/* pair_stage.cpp                                                         */
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
 * @file pair_stage.cpp
 * @brief Implements the per-pair stage: the taps, the direct sound and the tail's output.
 * @author Mustafa Garip & Sushi Systems
 */

#include <SushiDSP/nodes/room/pair_stage.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>

#include <SushiDSP/nodes/room/stage_limits.hpp>

namespace SushiDSP::Room
{
    namespace
    {
        /// @brief Bounds how far a tap's delay may move, in frames, for its state to be carried.
        constexpr float K_MATCH_DELAY_FRAMES = 2.0f;

        /// @brief Bounds how far any one of a tap's ten coefficients may move for its state to
        /// be carried. Measured in modules/nodes/room/README.md, "The two-bank crossfade".
        constexpr double K_MATCH_COEFFICIENT_MOVE = 0.2;

        /// @brief Returns the largest move among the two sections' coefficients.
        double coefficient_move(const room::EarlyTap& retiring,
                                const room::EarlyTap& arriving) noexcept
        {
            double move = 0.0;
            for (std::size_t index = 0; index < retiring.section_a.size(); ++index)
            {
                move = std::max(move,
                                std::fabs(retiring.section_a[index] - arriving.section_a[index]));
                move = std::max(move,
                                std::fabs(retiring.section_b[index] - arriving.section_b[index]));
            }
            return move;
        }

        /// @brief Returns true when an arriving tap is the retiring tap moved a little, its
        /// delay within K_MATCH_DELAY_FRAMES and every coefficient within K_MATCH_COEFFICIENT_MOVE.
        bool taps_match(const room::EarlyTap& retiring, const room::EarlyTap& arriving) noexcept
        {
            return std::fabs(retiring.delay_frames - arriving.delay_frames) <=
                       K_MATCH_DELAY_FRAMES &&
                   coefficient_move(retiring, arriving) <= K_MATCH_COEFFICIENT_MOVE;
        }
    } // namespace

    bool PairStage::take_direct_image(const room::RoomRenderSet& set,
                                      room::EarlyTapSet& reflections,
                                      room::EarlyTap& direct) noexcept
    {
        if (set.listener != room::ListenerKind::Ears || set.early.count == 0)
        {
            reflections = set.early;
            return false;
        }
        // For the ears the direct sound is the HRIR pair's alone; the bank holds reflections.
        direct = set.early.taps[0];
        reflections.count = set.early.count - 1u;
        reflections.direct_count = 0;
        reflections.crossover_time_s = set.early.crossover_time_s;
        reflections.scattered_energy = set.early.scattered_energy;
        std::copy(set.early.taps.begin() + 1,
                  set.early.taps.begin() + static_cast<std::ptrdiff_t>(set.early.count),
                  reflections.taps.begin());
        return true;
    }

    void PairStage::prepare(room::ListenerKind kind, double sample_rate,
                            std::uint32_t alignment_frames, std::uint32_t max_block_frames)
    {
        kind_ = kind;
        early_.prepare(max_early_delay_frames(sample_rate), max_block_frames);
        direct_path_.prepare(alignment_frames, max_block_frames);

        accumulation_.assign(static_cast<std::size_t>(K_BANKS) * K_CHANNELS * max_block_frames,
                             0.0f);
        for (std::uint32_t bank = 0; bank < K_BANKS; ++bank)
        {
            for (std::uint32_t channel = 0; channel < K_CHANNELS; ++channel)
            {
                accumulation_pointers_[bank][channel] =
                    accumulation_.data() +
                    (static_cast<std::size_t>(bank) * K_CHANNELS + channel) * max_block_frames;
            }
        }
        early_mono_.assign(max_block_frames, 0.0f);
        tail_retiring_.assign(max_block_frames, 0.0f);
        for (std::uint32_t index = 0; index < room::K_EARS; ++index)
        {
            direct_out_[index].assign(max_block_frames, 0.0f);
            retiring_direct_[index].assign(max_block_frames, 0.0f);
            tail_read_[index].assign(max_block_frames, 0.0f);
            tail_[index].assign(max_block_frames, 0.0f);
        }
        reset();
    }

    void PairStage::reset() noexcept
    {
        early_.reset();
        direct_path_.reset();
        tone_left_.reset();
        tone_right_.reset();
        coherence_a_.reset();
        coherence_b_.reset();
        for (room::EarlyTapSet& taps : bank_taps_)
        {
            taps.count = 0;
        }
        active_bank_ = 0;
        live_ = false;
        pending_waiting_ = false;
    }

    bool PairStage::live() const noexcept
    {
        return live_;
    }

    void PairStage::adopt(const room::RoomRenderSet& set) noexcept
    {
        pending_.tone = set.late.tone_correction;
        pending_.coherence_a = set.late.coherence_a;
        pending_.coherence_b = set.late.coherence_b;
        pending_waiting_ = true;

        // The delay and the level move under the fade; the cascades wait for its lag.
        const bool first = !live_;
        tail_delay_from_ = first ? set.late.delay_to_crossover_frames : tail_delay_to_;
        tail_gain_from_ = first ? set.late.input_gain : tail_gain_to_;
        tail_delay_to_ = set.late.delay_to_crossover_frames;
        tail_gain_to_ = set.late.input_gain;

        room::EarlyTap direct{};
        if (first)
        {
            const bool has_direct = take_direct_image(set, bank_taps_[active_bank_], direct);
            early_.clear_bank(active_bank_);
            direct_path_.clear_bank(active_bank_);
            if (has_direct)
            {
                direct_path_.load_bank(active_bank_, direct, set);
            }
            live_ = true;
            apply_pending_when_due(room::FadeWindow{0, 0, 0});
            return;
        }

        // A mid-fade set restarts from the active bank; see modules/nodes/room/source/README.md.
        const std::uint32_t arriving = active_bank_ == 0 ? 1u : 0u;
        const bool has_direct = take_direct_image(set, bank_taps_[arriving], direct);
        const room::EarlyTapSet& retiring = bank_taps_[active_bank_];
        const room::EarlyTapSet& reflections = bank_taps_[arriving];
        const std::uint32_t paired = std::min(retiring.count, reflections.count);
        early_.clear_bank(arriving);
        for (std::uint32_t index = 0; index < paired; ++index)
        {
            if (taps_match(retiring.taps[index], reflections.taps[index]))
            {
                early_.carry_tap(active_bank_, arriving, index);
            }
        }
        direct_path_.clear_bank(arriving);
        if (has_direct)
        {
            if (direct_path_.active(active_bank_) &&
                taps_match(direct_path_.tap(active_bank_), direct))
            {
                direct_path_.carry_bank(active_bank_, arriving);
            }
            direct_path_.load_bank(arriving, direct, set);
        }
        active_bank_ = arriving;
    }

    void PairStage::apply_pending_when_due(const room::FadeWindow& window) noexcept
    {
        const bool waits_lag = kind_ == room::ListenerKind::Ears;
        if (!pending_waiting_ || (waits_lag && window.done < window.lag))
        {
            return;
        }
        tone_left_.load(pending_.tone);
        tone_right_.load(pending_.tone);
        coherence_a_.load(pending_.coherence_a);
        coherence_b_.load(pending_.coherence_b);
        pending_waiting_ = false;
    }

    void PairStage::render(const room::DelayRing& ring, const room::DelayRing& tail_a,
                           const room::DelayRing& tail_b, std::uint32_t frames,
                           const room::FadeWindow& window) noexcept
    {
        apply_pending_when_due(window);
        render_early(ring, frames, window);
        render_tail(tail_a, tail_b, frames, window);
        if (kind_ == room::ListenerKind::Ears)
        {
            render_direct(ring, frames, window);
        }
    }

    void PairStage::render_early(const room::DelayRing& ring, std::uint32_t frames,
                                 const room::FadeWindow& window) noexcept
    {
        clear_accumulation(active_bank_, frames);
        early_.render(ring, bank_taps_[active_bank_], active_bank_,
                      accumulation_pointers_[active_bank_].data(), frames);
        float* const* const active = accumulation_pointers_[active_bank_].data();
        const bool ears = kind_ == room::ListenerKind::Ears;
        if (window.fading > 0)
        {
            const std::uint32_t retiring = active_bank_ == 0 ? 1u : 0u;
            clear_accumulation(retiring, frames);
            early_.render(ring, bank_taps_[retiring], retiring,
                          accumulation_pointers_[retiring].data(), frames);
            float* const* const old = accumulation_pointers_[retiring].data();
            // Unlagged: the decoder or the microphone's delay holds it back.
            const std::uint32_t channels = ears ? K_CHANNELS : 1u;
            for (std::uint32_t channel = 0; channel < channels; ++channel)
            {
                float* const __restrict to = active[channel];
                const float* const __restrict from = old[channel];
                for (std::uint32_t frame = 0; frame < window.fading; ++frame)
                {
                    const float weight = room::window_weight(window, frame, false);
                    to[frame] = from[frame] + weight * (to[frame] - from[frame]);
                }
            }
        }
        if (!ears)
        {
            std::copy(active[0], active[0] + frames, early_mono_.data());
        }
    }

    void PairStage::render_direct(const room::DelayRing& ring, std::uint32_t frames,
                                  const room::FadeWindow& window) noexcept
    {
        direct_path_.render(ring, active_bank_, direct_out_[0].data(), direct_out_[1].data(),
                            frames);
        if (window.fading == 0)
        {
            return;
        }
        const std::uint32_t retiring = active_bank_ == 0 ? 1u : 0u;
        direct_path_.render(ring, retiring, retiring_direct_[0].data(),
                            retiring_direct_[1].data(), frames);
        room::blend_pair(direct_out_[0].data(), direct_out_[1].data(), retiring_direct_[0].data(),
                         retiring_direct_[1].data(), window, true);
    }

    void PairStage::read_tail(const room::DelayRing& ring, float* output, std::uint32_t frames,
                              const room::FadeWindow& window) noexcept
    {
        float* const __restrict out = output;
        ring.read(static_cast<double>(tail_delay_to_), tail_gain_to_, out, frames);
        const bool moved = tail_delay_from_ != tail_delay_to_ || tail_gain_from_ != tail_gain_to_;
        if (window.fading == 0 || !moved)
        {
            return;
        }
        float* const __restrict old = tail_retiring_.data();
        ring.read(static_cast<double>(tail_delay_from_), tail_gain_from_, old, window.fading);
        // The ears' tail blends at the output; a microphone's block waits later.
        const bool lagged = kind_ == room::ListenerKind::Ears;
        for (std::uint32_t frame = 0; frame < window.fading; ++frame)
        {
            const float weight = room::window_weight(window, frame, lagged);
            out[frame] = old[frame] + weight * (out[frame] - old[frame]);
        }
    }

    void PairStage::render_tail(const room::DelayRing& tail_a, const room::DelayRing& tail_b,
                                std::uint32_t frames, const room::FadeWindow& window) noexcept
    {
        float* const __restrict sum_a = tail_read_[0].data();
        float* const __restrict left = tail_[0].data();
        read_tail(tail_a, sum_a, frames, window);
        if (kind_ != room::ListenerKind::Ears)
        {
            for (std::uint32_t frame = 0; frame < frames; ++frame)
            {
                left[frame] = tone_left_.process_sample(sum_a[frame]);
            }
            return;
        }
        float* const __restrict sum_b = tail_read_[1].data();
        float* const __restrict right = tail_[1].data();
        read_tail(tail_b, sum_b, frames, window);
        for (std::uint32_t frame = 0; frame < frames; ++frame)
        {
            left[frame] = tone_left_.process_sample(sum_a[frame]);
            const float mixed =
                coherence_a_.process_sample(sum_a[frame]) + coherence_b_.process_sample(sum_b[frame]);
            right[frame] = tone_right_.process_sample(mixed);
        }
    }

    void PairStage::clear_accumulation(std::uint32_t bank, std::uint32_t frames) noexcept
    {
        for (std::uint32_t channel = 0; channel < K_CHANNELS; ++channel)
        {
            std::fill_n(accumulation_pointers_[bank][channel], frames, 0.0f);
        }
    }

    const float* const* PairStage::ambisonic() const noexcept
    {
        return accumulation_pointers_[active_bank_].data();
    }

    const float* PairStage::direct(std::uint32_t ear) const noexcept
    {
        return direct_out_[ear == 0 ? 0 : 1].data();
    }

    const float* PairStage::tail(std::uint32_t ear) const noexcept
    {
        return tail_[ear == 0 ? 0 : 1].data();
    }

    const float* PairStage::early_mono() const noexcept
    {
        return early_mono_.data();
    }
} // namespace SushiDSP::Room
