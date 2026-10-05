/****************************************************************/
/* GeometricPath.cpp                                            */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file GeometricPath.cpp
 * @brief The tap pairing, the two-bank render and the routing the rays leave the path by.
 * @author Mustafa Garip & Sushi Systems
 */

#include "SushiDSP/room/GeometricPath.hpp"

#include <algorithm>
#include <cmath>

#include "SushiDSP/room/RoomScene.hpp"

namespace SushiDSP::room
{
    namespace
    {
        /// @brief Holds the reflection order every set is solved to, RoomRenderSet's own.
        constexpr std::uint32_t kImageOrder = 4;

        /// @brief Holds the longest line the tail may need, in seconds; the designer bounds
        /// its spread by the same number.
        constexpr double kMaxLineSeconds = FdnDesign::kMaxLineSeconds;

        /// @brief Holds the frames the direct path's ring keeps past the longest tap for the
        /// far ear's ITD, 2.7 ms at 48 kHz against Woodworth's 0.66 ms.
        constexpr std::uint32_t kItdMarginFrames = 128;

        /// @brief Bounds how far a tap's delay may move, in frames, for its state to be carried.
        constexpr float kMatchDelayFrames = 2.0f;

        /// @brief Bounds how far any one of a tap's ten coefficients may move for its state to
        /// be carried. Measured in modules/nodes/room/README.md, "The two-bank crossfade".
        constexpr double kMatchCoefficientMove = 0.2;

        /// @brief Returns the frames the longest t_c the scene bounds allow takes at one rate:
        /// (order + 1) * kMaxDimension / c, 8397 at 48 kHz.
        std::uint32_t max_early_delay_frames(double sample_rate) noexcept
        {
            const double seconds = static_cast<double>(kImageOrder + 1u)
                * RoomScene::kMaxDimension / kSpeedOfSound;
            return static_cast<std::uint32_t>(std::ceil(seconds * sample_rate));
        }

        /// @brief Returns the largest move among the two sections' coefficients.
        double coefficient_move(const EarlyTap& retiring, const EarlyTap& arriving) noexcept
        {
            double move = 0.0;
            for (std::size_t index = 0; index < retiring.section_a.size(); ++index)
            {
                move = std::max(move, std::fabs(retiring.section_a[index]
                                                - arriving.section_a[index]));
                move = std::max(move, std::fabs(retiring.section_b[index]
                                                - arriving.section_b[index]));
            }
            return move;
        }

        /// @brief Returns true when an arriving tap is the retiring tap moved a little, its
        /// delay within kMatchDelayFrames and every coefficient within kMatchCoefficientMove.
        bool taps_match(const EarlyTap& retiring, const EarlyTap& arriving) noexcept
        {
            return std::fabs(retiring.delay_frames - arriving.delay_frames) <= kMatchDelayFrames
                && coefficient_move(retiring, arriving) <= kMatchCoefficientMove;
        }
    }

    bool GeometricPath::take_direct_image(const RoomRenderSet& set, EarlyTapSet& reflections,
                                          EarlyTap& direct) noexcept
    {
        if (set.listener != ListenerKind::Ears || set.early.count == 0)
        {
            reflections = set.early;
            return false;
        }
        // For the ears the direct sound is the HRIR pair's alone; the bank holds reflections.
        direct = set.early.taps[0];
        reflections.count = set.early.count - 1u;
        reflections.crossover_time_s = set.early.crossover_time_s;
        reflections.scattered_energy = set.early.scattered_energy;
        std::copy(set.early.taps.begin() + 1,
                  set.early.taps.begin() + static_cast<std::ptrdiff_t>(set.early.count),
                  reflections.taps.begin());
        return true;
    }

    void GeometricPath::prepare(double sample_rate, std::uint32_t alignment_frames,
                                std::uint32_t max_block_frames)
    {
        const std::uint32_t max_delay = max_early_delay_frames(sample_rate);
        early_.prepare(max_delay, max_block_frames);
        ring_.prepare(alignment_frames + max_delay + kItdMarginFrames, max_block_frames);
        direct_.prepare(alignment_frames, max_block_frames);
        late_.prepare(static_cast<std::uint32_t>(std::ceil(kMaxLineSeconds * sample_rate)));

        accumulation_.assign(
            static_cast<std::size_t>(kBanks) * kChannels * max_block_frames, 0.0f);
        for (std::uint32_t bank = 0; bank < kBanks; ++bank)
        {
            for (std::uint32_t channel = 0; channel < kChannels; ++channel)
            {
                accumulation_pointers_[bank][channel] = accumulation_.data()
                    + (static_cast<std::size_t>(bank) * kChannels + channel) * max_block_frames;
            }
        }
        high_.assign(max_block_frames, 0.0f);
        tail_.assign(max_block_frames, 0.0f);
        tail_right_.assign(max_block_frames, 0.0f);
        tail_outputs_ = { tail_.data(), tail_.data() };
        tail_input_.assign(max_block_frames, 0.0f);
        tail_input_retiring_.assign(max_block_frames, 0.0f);
        early_mono_.assign(max_block_frames, 0.0f);
        direct_left_.assign(max_block_frames, 0.0f);
        direct_right_.assign(max_block_frames, 0.0f);
        retiring_direct_left_.assign(max_block_frames, 0.0f);
        retiring_direct_right_.assign(max_block_frames, 0.0f);
    }

    void GeometricPath::reset() noexcept
    {
        early_.reset();
        ring_.reset();
        direct_.reset();
        late_.reset();
    }

    float* GeometricPath::high_buffer() noexcept
    {
        return high_.data();
    }

    void GeometricPath::write_input(std::uint32_t frames) noexcept
    {
        early_.write(high_.data(), frames);
        ring_.write(high_.data(), frames);
    }

    bool GeometricPath::active_is_ears() const noexcept
    {
        return bank_listener_[active_bank_] == ListenerKind::Ears;
    }

    bool GeometricPath::listeners_differ() const noexcept
    {
        const std::uint32_t retiring = active_bank_ == 0 ? 1u : 0u;
        return bank_listener_[retiring] != bank_listener_[active_bank_];
    }

    bool GeometricPath::any_microphone() const noexcept
    {
        return bank_listener_[0] == ListenerKind::Microphone
            || bank_listener_[1] == ListenerKind::Microphone;
    }

    const float* GeometricPath::direct(std::uint32_t ear) const noexcept
    {
        return ear == 0 ? direct_left_.data() : direct_right_.data();
    }

    const float* GeometricPath::tail(std::uint32_t ear) const noexcept
    {
        return tail_outputs_[ear];
    }

    const float* GeometricPath::early_mono() const noexcept
    {
        return early_mono_.data();
    }

    void GeometricPath::stage_tail(const RoomRenderSet& set, bool first) noexcept
    {
        pending_.design = set.late;
        // The tail's delay to t_c is read out of the ring, so the tail itself waits nothing.
        pending_.design.delay_to_crossover_frames = 0.0f;
        pending_.two_ears = set.listener == ListenerKind::Ears;
        // The read delay moves under the fade from its lag on; the coefficients wait for it.
        tail_delay_from_ = first ? set.late.delay_to_crossover_frames : tail_delay_to_;
        tail_delay_to_ = set.late.delay_to_crossover_frames;
    }

    void GeometricPath::apply_pending() noexcept
    {
        tail_design_ = pending_.design;
        // The microphone's tail is one signal read twice; the ears' passes the coherence pair.
        tail_outputs_[1] = pending_.two_ears ? tail_right_.data() : tail_.data();
    }

    void GeometricPath::adopt_first(const RoomRenderSet& set) noexcept
    {
        EarlyTap direct{};
        const bool has_direct = take_direct_image(set, bank_taps_[active_bank_], direct);
        early_.clear_bank(active_bank_);
        direct_.clear_bank(active_bank_);
        if (has_direct)
        {
            direct_.load_bank(active_bank_, direct, set);
        }
        bank_listener_[active_bank_] = set.listener;
    }

    bool GeometricPath::adopt(const RoomRenderSet& set) noexcept
    {
        // A set arriving mid-fade restarts from the active bank; see modules/nodes/room/README.md.
        EarlyTap direct{};
        const std::uint32_t arriving = active_bank_ == 0 ? 1u : 0u;
        const bool has_direct = take_direct_image(set, bank_taps_[arriving], direct);
        const EarlyTapSet& retiring = bank_taps_[active_bank_];
        const EarlyTapSet& reflections = bank_taps_[arriving];
        const std::uint32_t paired = std::min(retiring.count, reflections.count);
        early_.clear_bank(arriving);
        for (std::uint32_t index = 0; index < paired; ++index)
        {
            if (taps_match(retiring.taps[index], reflections.taps[index]))
            {
                early_.carry_tap(active_bank_, arriving, index);
            }
        }
        direct_.clear_bank(arriving);
        if (has_direct)
        {
            if (direct_.active(active_bank_) && taps_match(direct_.tap(active_bank_), direct))
            {
                direct_.carry_bank(active_bank_, arriving);
            }
            direct_.load_bank(arriving, direct, set);
        }
        // The ears' fade starts when the decoder's first partition of the set comes out.
        const bool ears_involved = set.listener == ListenerKind::Ears
            || bank_listener_[active_bank_] == ListenerKind::Ears;
        bank_listener_[arriving] = set.listener;
        active_bank_ = arriving;
        return ears_involved;
    }

    void GeometricPath::render_early(std::uint32_t frames, const FadeWindow& window) noexcept
    {
        clear_accumulation(active_bank_, frames);
        early_.render(bank_taps_[active_bank_], active_bank_,
                      accumulation_pointers_[active_bank_].data(), frames);
        if (window.fading == 0)
        {
            return;
        }
        const std::uint32_t retiring = active_bank_ == 0 ? 1u : 0u;
        clear_accumulation(retiring, frames);
        early_.render(bank_taps_[retiring], retiring,
                      accumulation_pointers_[retiring].data(), frames);
    }

    void GeometricPath::render_tail(std::uint32_t frames, const FadeWindow& window) noexcept
    {
        read_tail_input(frames, window);
        late_.process(tail_design_, tail_input_.data(), tail_outputs_[0], tail_outputs_[1],
                      frames);
    }

    void GeometricPath::read_tail_input(std::uint32_t frames, const FadeWindow& window) noexcept
    {
        float* const __restrict input = tail_input_.data();
        ring_.read(static_cast<double>(tail_delay_to_), 1.0f, input, frames);
        if (window.fading == 0 || tail_delay_from_ == tail_delay_to_)
        {
            return;
        }
        float* const __restrict old = tail_input_retiring_.data();
        ring_.read(static_cast<double>(tail_delay_from_), 1.0f, old, window.fading);
        for (std::uint32_t frame = 0; frame < window.fading; ++frame)
        {
            const float weight = window_weight(window, frame, true);
            input[frame] = old[frame] + weight * (input[frame] - old[frame]);
        }
    }

    void GeometricPath::render_direct(std::uint32_t frames, const FadeWindow& window) noexcept
    {
        direct_.render(ring_, active_bank_, direct_left_.data(), direct_right_.data(), frames);
        if (window.fading == 0)
        {
            return;
        }
        const std::uint32_t retiring = active_bank_ == 0 ? 1u : 0u;
        direct_.render(ring_, retiring, retiring_direct_left_.data(),
                       retiring_direct_right_.data(), frames);
        blend_pair(direct_left_.data(), direct_right_.data(), retiring_direct_left_.data(),
                   retiring_direct_right_.data(), window, true);
    }

    const float* const* GeometricPath::route_early(std::uint32_t frames,
                                                   const FadeWindow& window) noexcept
    {
        const std::uint32_t retiring = active_bank_ == 0 ? 1u : 0u;
        const bool fading = window.fading > 0;
        const bool active_ears = bank_listener_[active_bank_] == ListenerKind::Ears;
        const bool retiring_ears = fading && bank_listener_[retiring] == ListenerKind::Ears;
        float* const __restrict mono = early_mono_.data();
        float* const* const active = accumulation_pointers_[active_bank_].data();
        float* const* const old = accumulation_pointers_[retiring].data();

        // The decoded channels read the fade unlagged and come out at the lag; channel 0 waits nothing.
        if (active_ears && (!fading || retiring_ears))
        {
            for (std::uint32_t channel = 0; channel < kChannels; ++channel)
            {
                for (std::uint32_t frame = 0; frame < window.fading; ++frame)
                {
                    const float weight = window_weight(window, frame, false);
                    active[channel][frame] = old[channel][frame]
                        + weight * (active[channel][frame] - old[channel][frame]);
                }
            }
            std::fill_n(mono, frames, 0.0f);
            return active;
        }
        if (active_ears)
        {
            // The ears arrive over the microphone: its channel 0 fades out on its own.
            for (std::uint32_t frame = 0; frame < window.fading; ++frame)
            {
                const float weight = window_weight(window, frame, false);
                for (std::uint32_t channel = 0; channel < kChannels; ++channel)
                {
                    active[channel][frame] *= weight;
                }
                mono[frame] = (1.0f - window_weight(window, frame, true)) * old[0][frame];
            }
            std::fill(mono + window.fading, mono + frames, 0.0f);
            return active;
        }
        if (retiring_ears)
        {
            // The microphone arrives over the ears: their sixteen channels fade out decoded.
            for (std::uint32_t frame = 0; frame < window.fading; ++frame)
            {
                const float weight = window_weight(window, frame, false);
                for (std::uint32_t channel = 0; channel < kChannels; ++channel)
                {
                    old[channel][frame] *= 1.0f - weight;
                }
                mono[frame] = window_weight(window, frame, true) * active[0][frame];
            }
            for (std::uint32_t channel = 0; channel < kChannels; ++channel)
            {
                std::fill(old[channel] + window.fading, old[channel] + frames, 0.0f);
            }
            std::copy(active[0] + window.fading, active[0] + frames, mono + window.fading);
            return old;
        }
        for (std::uint32_t frame = 0; frame < window.fading; ++frame)
        {
            const float weight = window_weight(window, frame, true);
            mono[frame] = old[0][frame] + weight * (active[0][frame] - old[0][frame]);
        }
        std::copy(active[0] + window.fading, active[0] + frames, mono + window.fading);
        return nullptr;
    }

    void GeometricPath::clear_accumulation(std::uint32_t set, std::uint32_t frames) noexcept
    {
        for (std::uint32_t channel = 0; channel < kChannels; ++channel)
        {
            std::fill_n(accumulation_pointers_[set][channel], frames, 0.0f);
        }
    }
}
