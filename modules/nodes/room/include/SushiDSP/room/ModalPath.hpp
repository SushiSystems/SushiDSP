/**************************************************************************/
/* ModalPath.hpp                                                          */
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
 * @file ModalPath.hpp
 * @brief The crossover's low half: the standing waves, their two banks and the swap between them.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "SushiDSP/room/FadeWindow.hpp"
#include "SushiDSP/room/ModalBank.hpp"
#include "SushiDSP/room/ModalDesign.hpp"

namespace SushiDSP::room
{
    /// @brief Renders the room's standing waves from the input's low half: two resonator banks,
    /// one per side of a crossfade, and the rule that decides between keeping a bank's state and
    /// ramping its gains or blending a fresh bank in. Owns the low half and the field it writes
    /// at each ear. Spec §9a.2.
    class ModalPath
    {
    public:
        /// @brief Counts the state banks, one per side of a crossfade.
        static constexpr std::uint32_t kBanks = 2;

        /// @brief Sizes both banks and every block buffer.
        void prepare(std::uint32_t max_block_frames);

        /// @brief Zeroes both banks and drops any running blend or ramp.
        void reset() noexcept;

        /// @brief Returns the buffer the input crossover writes the low half into.
        float* low_buffer() noexcept;

        /// @brief Takes the first design ever: the active bank renders it from zero state.
        void adopt_first(const ModalDesign& design) noexcept;

        /// @brief Adopts a design under the match rule: resonators identical to the active
        /// bank's keep that bank's state and ramp its gains over the fade; resonators that
        /// differ start the spare bank from zero and blend the two over the fade.
        void adopt(const ModalDesign& design) noexcept;

        /// @brief Runs the low half through the active bank, blending the retiring bank or
        /// ramping the gains under the fade, into the left and right fields; the fade is read
        /// unlagged for the ears, whose field waits the alignment delay after it.
        void render(std::uint32_t frames, const FadeWindow& window,
                    bool microphone_involved) noexcept;

        /// @brief Drops the running blend and ramp once the fade has finished.
        void settle() noexcept;

        /// @brief Returns the modal field at one ear, as the bank rendered it.
        const float* field(std::uint32_t ear) const noexcept;

        /// @brief Returns true when two designs hold the same resonators: the same count and
        /// every paired section equal bit for bit, as one room designs them whatever the poses.
        static bool same_resonators(const ModalDesign& retiring,
                                    const ModalDesign& arriving) noexcept;

    private:
        /// @brief Runs the active bank over one frame with its gains at the fade's weight
        /// between the retiring and the arriving design.
        void render_ramp(std::uint32_t frame, float weight) noexcept;

        /// @brief Renders the standing waves, one bank per side of a crossfade.
        std::array<ModalBank, kBanks> banks_;

        /// @brief Stores the design each bank renders; copied in, never borrowed from a set.
        std::array<ModalDesign, kBanks> bank_modes_{};

        /// @brief Stores the gains the active bank ramps from, the design rendered last.
        ModalDesign from_{};

        /// @brief Stores the active bank's resonators with the gains at the ramp's step.
        ModalDesign blend_{};

        /// @brief Indexes the bank rendering the set in place.
        std::uint32_t active_bank_ = 0;

        /// @brief Records that the retiring bank blends out under the running fade.
        bool fading_ = false;

        /// @brief Records that the active bank's gains ramp under the running fade.
        bool ramping_ = false;

        /// @brief Stores the input's low half, the bank's feed.
        std::vector<float> low_;

        /// @brief Stores the modal field at the left ear.
        std::vector<float> left_;

        /// @brief Stores the modal field at the right ear.
        std::vector<float> right_;

        /// @brief Stores the retiring bank's left ear while it fades out.
        std::vector<float> retiring_left_;

        /// @brief Stores the retiring bank's right ear while it fades out.
        std::vector<float> retiring_right_;
    };
}
