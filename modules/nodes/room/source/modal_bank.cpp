/**************************************************************************/
/* modal_bank.cpp                                                         */
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
 * @file modal_bank.cpp
 * @brief The lane-grouped resonator kernel and the per-frame reduction into the two ears.
 * @author Mustafa Garip & Sushi Systems
 */

#include "SushiDSP/room/modal_bank.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>

#include "SushiDSP/core/denormal_guard.hpp"

namespace SushiDSP::room
{
    namespace
    {
        /// @brief Counts the lanes one group advances.
        constexpr std::size_t K_LANES = ModalBank::K_LANES;

        /// @brief Holds K_LANES modes' coefficients side by side, the source gain folded into the
        /// numerator; a lane at or past the design's count holds zeros and stays silent.
        struct LaneGroup
        {
                /// @brief Stores b0 times the source gain per lane.
                double feed_now[K_LANES];

                /// @brief Stores b1 times the source gain per lane.
                double feed_previous[K_LANES];

                /// @brief Stores b2 times the source gain per lane.
                double feed_oldest[K_LANES];

                /// @brief Stores a1 per lane.
                double pole_first[K_LANES];

                /// @brief Stores a2 per lane.
                double pole_second[K_LANES];

                /// @brief Stores psi_n at the left ear per lane.
                double ear_left[K_LANES];

                /// @brief Stores psi_n at the right ear per lane.
                double ear_right[K_LANES];
        };

        /// @brief Fills a group from the design's modes starting at first, zeroing every lane at
        /// or past count.
        void gather_group(const ModalDesign& design, std::uint32_t first, LaneGroup& group) noexcept
        {
            for (std::size_t lane = 0; lane < K_LANES; ++lane)
            {
                const std::uint32_t index = first + static_cast<std::uint32_t>(lane);
                const bool live = index < design.count;
                const ModalDesign::Mode& mode = design.modes[live ? index : 0];
                const double source = live ? static_cast<double>(mode.source_gain) : 0.0;
                group.feed_now[lane] = live ? mode.section[0] * source : 0.0;
                group.feed_previous[lane] = live ? mode.section[1] * source : 0.0;
                group.feed_oldest[lane] = live ? mode.section[2] * source : 0.0;
                group.pole_first[lane] = live ? mode.section[3] : 0.0;
                group.pole_second[lane] = live ? mode.section[4] : 0.0;
                group.ear_left[lane] = live ? static_cast<double>(mode.ear_gain[0]) : 0.0;
                group.ear_right[lane] = live ? static_cast<double>(mode.ear_gain[1]) : 0.0;
            }
        }

        /// @brief Advances one group's K_LANES resonators over frames samples in transposed direct
        /// form II, adding each lane's output times its ear gains into the lane partials.
        void run_group(const LaneGroup& group, double* const __restrict state_first,
                       double* const __restrict state_second, const float* const __restrict input,
                       double* const __restrict partial_left,
                       double* const __restrict partial_right, std::uint32_t frames) noexcept
        {
            double first[K_LANES];
            double second[K_LANES];
            for (std::size_t lane = 0; lane < K_LANES; ++lane)
            {
                first[lane] = state_first[lane];
                second[lane] = state_second[lane];
            }
            for (std::uint32_t frame = 0; frame < frames; ++frame)
            {
                const double sample = static_cast<double>(input[frame]);
                double* const left = partial_left + static_cast<std::size_t>(frame) * K_LANES;
                double* const right = partial_right + static_cast<std::size_t>(frame) * K_LANES;
                for (std::size_t lane = 0; lane < K_LANES; ++lane)
                {
                    const double output = group.feed_now[lane] * sample + first[lane];
                    first[lane] = group.feed_previous[lane] * sample -
                                  group.pole_first[lane] * output + second[lane];
                    second[lane] =
                        group.feed_oldest[lane] * sample - group.pole_second[lane] * output;
                    left[lane] += output * group.ear_left[lane];
                    right[lane] += output * group.ear_right[lane];
                }
            }
            for (std::size_t lane = 0; lane < K_LANES; ++lane)
            {
                state_first[lane] = first[lane];
                state_second[lane] = second[lane];
            }
        }

        /// @brief Sums the K_LANES partials of every frame into the ear's output buffer.
        void reduce_lanes(const double* const __restrict partial, float* const __restrict output,
                          std::uint32_t frames) noexcept
        {
            for (std::uint32_t frame = 0; frame < frames; ++frame)
            {
                const double* const lanes = partial + static_cast<std::size_t>(frame) * K_LANES;
                double sum = 0.0;
                for (std::size_t lane = 0; lane < K_LANES; ++lane)
                {
                    sum += lanes[lane];
                }
                output[frame] += static_cast<float>(sum);
            }
        }
    } // namespace

    void ModalBank::prepare(std::uint32_t max_block_frames)
    {
        // The state holds whole groups; the last group's spare lanes read real storage.
        const std::size_t padded_modes =
            (ModalDesign::K_MAX_MODES + K_LANES - 1) / K_LANES * K_LANES;
        state_first_.assign(padded_modes, 0.0);
        state_second_.assign(padded_modes, 0.0);
        partial_left_.assign(static_cast<std::size_t>(max_block_frames) * K_LANES, 0.0);
        partial_right_.assign(static_cast<std::size_t>(max_block_frames) * K_LANES, 0.0);
        max_block_frames_ = max_block_frames;
    }

    void ModalBank::reset() noexcept
    {
        std::fill(state_first_.begin(), state_first_.end(), 0.0);
        std::fill(state_second_.begin(), state_second_.end(), 0.0);
    }

    void ModalBank::process(const ModalDesign& design, const float* input, float* left,
                            float* right, std::uint32_t frames) noexcept
    {
        if (design.count == 0u || max_block_frames_ == 0u)
        {
            return;
        }
        const DenormalGuard guard;
        std::uint32_t done = 0;
        while (done < frames)
        {
            const std::uint32_t chunk = std::min(frames - done, max_block_frames_);
            render_block(design, input + done, left + done, right + done, chunk);
            done += chunk;
        }
    }

    void ModalBank::render_block(const ModalDesign& design, const float* input, float* left,
                                 float* right, std::uint32_t frames) noexcept
    {
        const std::size_t partial_count = static_cast<std::size_t>(frames) * K_LANES;
        std::fill_n(partial_left_.data(), partial_count, 0.0);
        std::fill_n(partial_right_.data(), partial_count, 0.0);
        const std::uint32_t count = std::min<std::uint32_t>(
            design.count, static_cast<std::uint32_t>(ModalDesign::K_MAX_MODES));
        LaneGroup group;
        for (std::uint32_t first = 0; first < count; first += static_cast<std::uint32_t>(K_LANES))
        {
            gather_group(design, first, group);
            run_group(group, state_first_.data() + first, state_second_.data() + first, input,
                      partial_left_.data(), partial_right_.data(), frames);
        }
        reduce_lanes(partial_left_.data(), left, frames);
        reduce_lanes(partial_right_.data(), right, frames);
    }
} // namespace SushiDSP::room
