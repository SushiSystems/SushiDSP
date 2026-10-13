/**************************************************************************/
/* pair_stage.hpp                                                         */
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
 * @file pair_stage.hpp
 * @brief Declares the stage that renders one cabinet at one receiver above the seam.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "SushiDSP/room/cascade_filter.hpp"
#include "SushiDSP/room/delay_ring.hpp"
#include "SushiDSP/room/direct_path.hpp"
#include "SushiDSP/room/early_reflection_renderer.hpp"
#include "SushiDSP/room/early_tap_set.hpp"
#include "SushiDSP/room/fade_window.hpp"
#include "SushiDSP/room/fdn_design.hpp"
#include "SushiDSP/room/image_source_solver.hpp"
#include "SushiDSP/room/room_render_set.hpp"

namespace SushiDSP::Room
{
    /// @brief Renders what one receiver hears of one cabinet above the seam: the direct sound, the
    /// image-source taps and the tail read at this pair's delay and level. Holds two state banks,
    /// one per side of a crossfade. Audio thread; allocation-free after prepare().
    ///
    /// The receiver's kind is fixed at prepare(). An ears pair hands on sixteen ambisonic channels,
    /// a direct pair and a tail pair; a microphone pair hands on one mono block.
    class PairStage
    {
        public:
            /// @brief Counts the channels the early renderer accumulates into.
            static constexpr std::uint32_t K_CHANNELS =
                room::EarlyReflectionRenderer::K_OUTPUT_CHANNELS;

            /// @brief Counts the state banks, one per side of a crossfade.
            static constexpr std::uint32_t K_BANKS = room::EarlyReflectionRenderer::K_BANK_COUNT;

            /// @brief Sizes the early renderer, the direct path and every block buffer.
            /// @param kind The receiver this pair renders for, for the life of the stage.
            /// @param sample_rate The rate in hertz.
            /// @param alignment_frames The decoder's partition, the frames the ears' paths wait.
            /// @param max_block_frames The longest block.
            void prepare(room::ListenerKind kind, double sample_rate,
                         std::uint32_t alignment_frames, std::uint32_t max_block_frames);

            /// @brief Silences every state and forgets every adopted set.
            void reset() noexcept;

            /// @brief Reports whether the stage has adopted a set since its last reset.
            /// @return True once adopt() has run.
            bool live() const noexcept;

            /// @brief Takes one pair's set. The first set after a reset is taken from zero state;
            /// a later one is loaded into the spare bank, carrying the state of every tap it
            /// keeps, and the tail's delay and level start moving under the fade.
            /// @param set The pair's set, built for this stage's receiver kind.
            void adopt(const room::RoomRenderSet& set) noexcept;

            /// @brief Renders one block: the taps of the active bank and, under a fade, of the
            /// retiring one; the direct sound; and the tail read out of the cabinet's two rings.
            /// @param ring The cabinet's high-half ring.
            /// @param tail_a The ring holding the network's A sum.
            /// @param tail_b The ring holding the network's B sum.
            /// @param frames The block length.
            /// @param window The block's fade window.
            void render(const room::DelayRing& ring, const room::DelayRing& tail_a,
                        const room::DelayRing& tail_b, std::uint32_t frames,
                        const room::FadeWindow& window) noexcept;

            /// @brief Returns an ears pair's sixteen channels, blended across the fade unlagged.
            /// @return K_CHANNELS pointers to one block each.
            const float* const* ambisonic() const noexcept;

            /// @brief Returns an ears pair's direct sound at one ear, blended across the fade.
            /// @param ear 0 for left, 1 for right.
            /// @return One block.
            const float* direct(std::uint32_t ear) const noexcept;

            /// @brief Returns the tail at one ear; a microphone pair's tail is at ear 0.
            /// @param ear 0 for left, 1 for right.
            /// @return One block.
            const float* tail(std::uint32_t ear) const noexcept;

            /// @brief Returns a microphone pair's taps, direct sound included, as one mono block.
            /// @return One block.
            const float* early_mono() const noexcept;

            /// @brief Copies a set's taps into a bank: an ears set's tap 0, the (0, 0, 0) image the
            /// HRIR pair renders, goes into direct and the rest into reflections; a microphone
            /// set's taps all go into reflections and direct is left as it was.
            /// @param set The set to read.
            /// @param reflections Receives the taps the bank renders.
            /// @param direct Receives tap 0 of an ears set.
            /// @pre The taps are sorted by arrival, so tap 0 is the direct image whenever count > 0.
            /// @return True when direct was written.
            static bool take_direct_image(const room::RoomRenderSet& set,
                                          room::EarlyTapSet& reflections,
                                          room::EarlyTap& direct) noexcept;

        private:
            /// @brief Holds what the tail's output takes from a set.
            struct TailAdoption
            {
                    /// @brief Stores the tone-correction cascade.
                    room::FdnDesign::Cascade tone;

                    /// @brief Stores the coherence filter gamma.
                    room::FdnDesign::Cascade coherence_a;

                    /// @brief Stores the sqrt(1 - gamma^2) filter.
                    room::FdnDesign::Cascade coherence_b;
            };

            /// @brief Loads the staged output cascades once the fade has run its lag; a microphone
            /// pair, which waits no lag, loads them at once.
            void apply_pending_when_due(const room::FadeWindow& window) noexcept;

            /// @brief Renders the active bank's taps and, under a fade, the retiring bank's, out of
            /// the cabinet's ring, then blends them: sixteen channels for the ears, channel 0 for a
            /// microphone.
            void render_early(const room::DelayRing& ring, std::uint32_t frames,
                              const room::FadeWindow& window) noexcept;

            /// @brief Renders the active bank's direct sound, blended with the retiring bank's.
            void render_direct(const room::DelayRing& ring, std::uint32_t frames,
                               const room::FadeWindow& window) noexcept;

            /// @brief Reads one of the network's sums at this pair's delay and level, blended with
            /// the read at the retiring delay and level under the fade.
            void read_tail(const room::DelayRing& ring, float* output, std::uint32_t frames,
                           const room::FadeWindow& window) noexcept;

            /// @brief Reads the tail out of the two rings and runs it through the output cascades.
            void render_tail(const room::DelayRing& tail_a, const room::DelayRing& tail_b,
                             std::uint32_t frames, const room::FadeWindow& window) noexcept;

            /// @brief Zeroes one accumulation set over the block.
            void clear_accumulation(std::uint32_t bank, std::uint32_t frames) noexcept;

            /// @brief Names the receiver this pair renders for.
            room::ListenerKind kind_ = room::ListenerKind::Ears;

            /// @brief Records that a set has been adopted since the last reset.
            bool live_ = false;

            /// @brief Reads the taps out of the cabinet's ring.
            room::EarlyReflectionRenderer early_;

            /// @brief Renders the ears' direct sound.
            room::DirectPath direct_path_;

            /// @brief Stores the taps each state bank renders, copied in so a reclaim can free the
            /// set at any time.
            std::array<room::EarlyTapSet, K_BANKS> bank_taps_{};

            /// @brief Indexes the bank rendering the set in place.
            std::uint32_t active_bank_ = 0;

            /// @brief Stores K_BANKS sets of K_CHANNELS accumulation buffers of one block each.
            std::vector<float> accumulation_;

            /// @brief Points at each accumulation set's K_CHANNELS buffers.
            std::array<std::array<float*, K_CHANNELS>, K_BANKS> accumulation_pointers_{};

            /// @brief Stores the microphone's channel 0, blended across the fade.
            std::vector<float> early_mono_;

            /// @brief Stores the direct sound at each ear, blended across the fade.
            std::array<std::vector<float>, room::K_EARS> direct_out_{};

            /// @brief Stores the retiring bank's direct sound at each ear while it fades out.
            std::array<std::vector<float>, room::K_EARS> retiring_direct_{};

            /// @brief Stores the network's A and B sums as this pair reads them.
            std::array<std::vector<float>, 2> tail_read_{};

            /// @brief Stores the retiring read of one sum while a fade runs.
            std::vector<float> tail_retiring_;

            /// @brief Stores the tail at each ear; a microphone pair fills ear 0 alone.
            std::array<std::vector<float>, room::K_EARS> tail_{};

            /// @brief Stores the delay to the crossover time the tail is read at, in frames.
            float tail_delay_to_ = 0.0f;

            /// @brief Stores the retiring set's delay, read under the fade.
            float tail_delay_from_ = 0.0f;

            /// @brief Stores the level the tail is read at.
            float tail_gain_to_ = 0.0f;

            /// @brief Stores the retiring set's level, read under the fade.
            float tail_gain_from_ = 0.0f;

            /// @brief Stores the output cascades waiting for the lagged fade to begin.
            TailAdoption pending_{};

            /// @brief Records that pending_ has not been loaded yet.
            bool pending_waiting_ = false;

            /// @brief Applies the tone correction to the left ear, or to the microphone.
            CascadeFilter tone_left_{};

            /// @brief Applies the tone correction to the right ear.
            CascadeFilter tone_right_{};

            /// @brief Applies the coherence filter gamma to the A sum on its way to the right ear.
            CascadeFilter coherence_a_{};

            /// @brief Applies sqrt(1 - gamma^2) to the B sum on its way to the right ear.
            CascadeFilter coherence_b_{};
    };
} // namespace SushiDSP::Room
