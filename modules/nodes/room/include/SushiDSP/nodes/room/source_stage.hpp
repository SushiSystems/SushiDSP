/**************************************************************************/
/* source_stage.hpp                                                       */
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
 * @file source_stage.hpp
 * @brief Declares the stage that renders what one cabinet puts into the room for every receiver.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include <SushiDSP/nodes/room/crossover_pair.hpp>
#include <SushiDSP/nodes/room/delay_ring.hpp>
#include <SushiDSP/nodes/room/environment_render_set.hpp>
#include <SushiDSP/nodes/room/fade_window.hpp>
#include <SushiDSP/nodes/room/fdn_design.hpp>
#include <SushiDSP/nodes/room/late_reverb.hpp>
#include <SushiDSP/nodes/room/modal_bank.hpp>
#include <SushiDSP/nodes/room/modal_design.hpp>

namespace SushiDSP::Room
{
    /// @brief Renders what one cabinet puts into the room before any receiver hears it: the input
    /// split at the seam, the high half held in a ring, the late tail's network with its two sums
    /// held in rings, and the modal resonators read at every receiver. Modal rows 0 and 1 are the
    /// listener's ears; row 2 + m is microphone m. Audio thread; allocation-free after prepare().
    class SourceStage
    {
        public:
            /// @brief Counts the modal read-out rows: the two ears and the microphones.
            static constexpr std::uint32_t K_ROWS = room::ModalBank::K_MAX_READ_OUTS;

            /// @brief Counts the modal state banks, one per side of a crossfade.
            static constexpr std::uint32_t K_BANKS = 2;

            /// @brief Sizes the rings, the network's lines, both modal banks and every block buffer.
            /// @param sample_rate The rate in hertz.
            /// @param alignment_frames The decoder's partition, the frames the ears' paths wait.
            /// @param max_block_frames The longest block.
            void prepare(double sample_rate, std::uint32_t alignment_frames,
                         std::uint32_t max_block_frames);

            /// @brief Silences the crossover, the rings, the network and both modal banks, and
            /// forgets every adopted design.
            void reset() noexcept;

            /// @brief Reports whether the stage has adopted a set since its last reset.
            /// @return True once adopt() has run.
            bool live() const noexcept;

            /// @brief Takes one cabinet's part of a set: the crossover corner, the tail's network
            /// and the modal resonators with every receiver's gain row. The first set after a
            /// reset is taken from zero state; a later one blends or ramps under the fade.
            /// @param set The set arriving.
            /// @param source The cabinet's index in the set.
            void adopt(const EnvironmentRenderSet& set, std::uint32_t source) noexcept;

            /// @brief Puts the staged network design in place once the fade has run its lag.
            /// @param window The block's fade window.
            void apply_pending_when_due(const room::FadeWindow& window) noexcept;

            /// @brief Splits one block at the seam and writes the high half into the ring.
            /// @param input The cabinet's block, or nullptr for silence.
            /// @param frames The block length.
            void write_input(const float* input, std::uint32_t frames) noexcept;

            /// @brief Runs the network over the high half and writes its two sums into their rings.
            /// @param frames The block length.
            void render_tail(std::uint32_t frames) noexcept;

            /// @brief Runs the low half through the modal bank into every row, blending the
            /// retiring bank or ramping the gains under the fade. Every row reads the fade
            /// unlagged: each receiver's field waits the alignment delay after it.
            /// @param frames The block length.
            /// @param window The block's fade window.
            void render_modal(std::uint32_t frames, const room::FadeWindow& window) noexcept;

            /// @brief Drops the running modal blend and ramp once the fade has finished.
            void settle() noexcept;

            /// @brief Returns the high half of the block last written.
            /// @return One block.
            const float* high() const noexcept;

            /// @brief Returns the ring that holds the high half.
            /// @return The ring the direct paths read.
            const room::DelayRing& ring() const noexcept;

            /// @brief Returns the ring that holds one of the network's two sums.
            /// @param sum 0 for the A sum, 1 for the B sum.
            /// @return The ring.
            const room::DelayRing& tail_ring(std::uint32_t sum) const noexcept;

            /// @brief Returns the modal field at one read-out row for the block last rendered.
            /// @param row The row, below K_ROWS.
            /// @return One block.
            const float* modal(std::uint32_t row) const noexcept;

            /// @brief Reports whether two designs hold the same resonators: the same count and
            /// every paired section equal bit for bit, as one room designs them whatever the poses.
            /// @param retiring The design in place.
            /// @param arriving The design arriving.
            /// @return True when no resonator differs.
            static bool same_resonators(const room::ModalDesign& retiring,
                                        const room::ModalDesign& arriving) noexcept;

        private:
            /// @brief Holds one modal design with every receiver's gain row.
            struct ModalSet
            {
                    /// @brief Stores the resonators and their source gains.
                    room::ModalDesign design{};

                    /// @brief Stores K_ROWS rows of K_MAX_MODES gains, row-major.
                    std::vector<float> gains;

                    /// @brief Counts the rows in use.
                    std::uint32_t rows = 0;
            };

            /// @brief Copies one cabinet's resonators and gain rows out of a set.
            static void load_modal(const EnvironmentRenderSet& set, std::uint32_t source,
                                   ModalSet& into) noexcept;

            /// @brief Reports whether two modal sets differ in a source gain, a row gain or the
            /// row count.
            static bool gains_moved(const ModalSet& retiring, const ModalSet& arriving) noexcept;

            /// @brief Runs one bank over a run of frames into the row outputs, offset by the
            /// frames already rendered.
            void run_bank(std::uint32_t bank, const ModalSet& modal, float* outputs,
                          std::uint32_t offset, std::uint32_t frames) noexcept;

            /// @brief Runs the active bank over one frame with its gains at the fade's weight
            /// between the retiring and the arriving set.
            void render_ramp(std::uint32_t frame, float weight) noexcept;

            /// @brief Stores the rate prepare() ran at, in hertz.
            double sample_rate_ = 48000.0;

            /// @brief Bounds the block length prepare() sized the buffers for.
            std::uint32_t max_block_ = 0;

            /// @brief Records that a set has been adopted since the last reset.
            bool live_ = false;

            /// @brief Splits the input at the seam.
            room::CrossoverPair crossover_;

            /// @brief Stores the input's low half, the modal bank's feed.
            std::vector<float> low_;

            /// @brief Stores the input's high half, the geometric layers' feed.
            std::vector<float> high_;

            /// @brief Holds the high half for the direct paths.
            room::DelayRing ring_;

            /// @brief Renders the tail's network.
            room::LateReverb network_;

            /// @brief Stores the network design in place.
            room::FdnDesign network_design_{};

            /// @brief Stores the network design waiting for the lagged fade to begin.
            room::FdnDesign pending_design_{};

            /// @brief Records that pending_design_ has not been put in place yet.
            bool pending_ = false;

            /// @brief Stores one block of each of the network's two sums.
            std::array<std::vector<float>, 2> tail_sums_{};

            /// @brief Holds the network's two sums for the receivers to read at their own delays.
            std::array<room::DelayRing, 2> tail_rings_{};

            /// @brief Renders the standing waves, one bank per side of a crossfade.
            std::array<room::ModalBank, K_BANKS> banks_;

            /// @brief Stores the modal set each bank renders.
            std::array<ModalSet, K_BANKS> bank_modal_{};

            /// @brief Stores the gains the active bank ramps from.
            ModalSet from_{};

            /// @brief Stores the active bank's resonators with the gains at the ramp's step.
            ModalSet blend_{};

            /// @brief Indexes the bank rendering the set in place.
            std::uint32_t active_bank_ = 0;

            /// @brief Records that the retiring bank blends out under the running fade.
            bool fading_ = false;

            /// @brief Records that the active bank's gains ramp under the running fade.
            bool ramping_ = false;

            /// @brief Stores K_ROWS blocks of the modal field, row-major.
            std::vector<float> modal_;

            /// @brief Stores K_ROWS blocks of the retiring bank's field while it fades out.
            std::vector<float> retiring_modal_;
    };
} // namespace SushiDSP::Room
