/**************************************************************************/
/* ModalBank.cpp                                                          */
/**************************************************************************/
/*                         This file is part of:                          */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                        https://sushisystems.io                         */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License");        */
/* you may not use this file except in compliance with the License.       */
/* You may obtain a copy of the License at                                */
/*                                                                        */
/*     http://www.apache.org/licenses/LICENSE-2.0                         */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

/**
 * @file ModalBank.cpp
 * @brief The lane-grouped resonator kernel and the per-frame reduction into the two ears.
 * @author Mustafa Garip & Sushi Systems
 */

#include "SushiDSP/room/ModalBank.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>

#include "SushiDSP/core/DenormalGuard.hpp"

namespace SushiDSP::room
{
    namespace
    {
        /// @brief Counts the lanes one group advances.
        constexpr std::size_t kLanes = ModalBank::kLanes;

        /// @brief Holds kLanes modes' coefficients side by side, the source gain folded into the
        /// numerator; a lane at or past the design's count holds zeros and stays silent.
        struct LaneGroup
        {
            /// @brief Stores b0 times the source gain per lane.
            double feed_now[kLanes];

            /// @brief Stores b1 times the source gain per lane.
            double feed_previous[kLanes];

            /// @brief Stores b2 times the source gain per lane.
            double feed_oldest[kLanes];

            /// @brief Stores a1 per lane.
            double pole_first[kLanes];

            /// @brief Stores a2 per lane.
            double pole_second[kLanes];

            /// @brief Stores psi_n at the left ear per lane.
            double ear_left[kLanes];

            /// @brief Stores psi_n at the right ear per lane.
            double ear_right[kLanes];
        };

        /// @brief Fills a group from the design's modes starting at first, zeroing every lane at
        /// or past count.
        void gather_group(const ModalDesign& design, std::uint32_t first, LaneGroup& group) noexcept
        {
            for (std::size_t lane = 0; lane < kLanes; ++lane)
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

        /// @brief Advances one group's kLanes resonators over frames samples in transposed direct
        /// form II, adding each lane's output times its ear gains into the lane partials.
        void run_group(const LaneGroup& group,
                       double* const __restrict state_first,
                       double* const __restrict state_second,
                       const float* const __restrict input,
                       double* const __restrict partial_left,
                       double* const __restrict partial_right,
                       std::uint32_t frames) noexcept
        {
            double first[kLanes];
            double second[kLanes];
            for (std::size_t lane = 0; lane < kLanes; ++lane)
            {
                first[lane] = state_first[lane];
                second[lane] = state_second[lane];
            }
            for (std::uint32_t frame = 0; frame < frames; ++frame)
            {
                const double sample = static_cast<double>(input[frame]);
                double* const left = partial_left + static_cast<std::size_t>(frame) * kLanes;
                double* const right = partial_right + static_cast<std::size_t>(frame) * kLanes;
                for (std::size_t lane = 0; lane < kLanes; ++lane)
                {
                    const double output = group.feed_now[lane] * sample + first[lane];
                    first[lane] = group.feed_previous[lane] * sample
                        - group.pole_first[lane] * output + second[lane];
                    second[lane] = group.feed_oldest[lane] * sample
                        - group.pole_second[lane] * output;
                    left[lane] += output * group.ear_left[lane];
                    right[lane] += output * group.ear_right[lane];
                }
            }
            for (std::size_t lane = 0; lane < kLanes; ++lane)
            {
                state_first[lane] = first[lane];
                state_second[lane] = second[lane];
            }
        }

        /// @brief Sums the kLanes partials of every frame into the ear's output buffer.
        void reduce_lanes(const double* const __restrict partial,
                          float* const __restrict output,
                          std::uint32_t frames) noexcept
        {
            for (std::uint32_t frame = 0; frame < frames; ++frame)
            {
                const double* const lanes = partial + static_cast<std::size_t>(frame) * kLanes;
                double sum = 0.0;
                for (std::size_t lane = 0; lane < kLanes; ++lane)
                {
                    sum += lanes[lane];
                }
                output[frame] += static_cast<float>(sum);
            }
        }
    }

    void ModalBank::prepare(std::uint32_t max_block_frames)
    {
        // The state holds whole groups; the last group's spare lanes read real storage.
        const std::size_t padded_modes = (ModalDesign::kMaxModes + kLanes - 1) / kLanes * kLanes;
        state_first_.assign(padded_modes, 0.0);
        state_second_.assign(padded_modes, 0.0);
        partial_left_.assign(static_cast<std::size_t>(max_block_frames) * kLanes, 0.0);
        partial_right_.assign(static_cast<std::size_t>(max_block_frames) * kLanes, 0.0);
        max_block_frames_ = max_block_frames;
    }

    void ModalBank::reset() noexcept
    {
        std::fill(state_first_.begin(), state_first_.end(), 0.0);
        std::fill(state_second_.begin(), state_second_.end(), 0.0);
    }

    void ModalBank::process(const ModalDesign& design, const float* input,
                            float* left, float* right, std::uint32_t frames) noexcept
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

    void ModalBank::render_block(const ModalDesign& design, const float* input,
                                 float* left, float* right, std::uint32_t frames) noexcept
    {
        const std::size_t partial_count = static_cast<std::size_t>(frames) * kLanes;
        std::fill_n(partial_left_.data(), partial_count, 0.0);
        std::fill_n(partial_right_.data(), partial_count, 0.0);
        const std::uint32_t count = std::min<std::uint32_t>(
            design.count, static_cast<std::uint32_t>(ModalDesign::kMaxModes));
        LaneGroup group;
        for (std::uint32_t first = 0; first < count; first += static_cast<std::uint32_t>(kLanes))
        {
            gather_group(design, first, group);
            run_group(group, state_first_.data() + first, state_second_.data() + first, input,
                      partial_left_.data(), partial_right_.data(), frames);
        }
        reduce_lanes(partial_left_.data(), left, frames);
        reduce_lanes(partial_right_.data(), right, frames);
    }
}
