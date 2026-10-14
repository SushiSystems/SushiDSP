/**************************************************************************/
/* modal_bank.hpp                                                         */
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
 * @file modal_bank.hpp
 * @brief The bank of second-order resonators that renders the room's standing waves.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include <SushiDSP/nodes/room/modal_design.hpp>

namespace SushiDSP::room
{
    /// @brief Up to K_MAX_MODES second-order resonators, each fed by the source's mode shape and
    /// read at as many receiver points as the caller names. The resonators run once whatever the
    /// count of read-outs. Audio thread. Spec §9a.2.
    class ModalBank
    {
        public:
            /// @brief Counts the modes one pass of the inner loop advances side by side.
            static constexpr std::size_t K_LANES = 4;

            /// @brief Bounds the read-outs one process() call takes: two ears and eight microphones.
            static constexpr std::uint32_t K_MAX_READ_OUTS = 10;

            /// @brief Names one receiver point: the mode shapes there and the block it is heard in.
            struct ReadOut
            {
                    /// @brief Points at psi_n at the receiver, one gain per mode, K_MAX_MODES long.
                    const float* gains;

                    /// @brief Points at the block the field at this receiver is added into.
                    float* output;
            };

            /// @brief Sizes the state for K_MAX_MODES and the lane scratch for one block; control
            /// thread, before the first process().
            /// @param max_block_frames The longest block one pass renders.
            void prepare(std::uint32_t max_block_frames);

            /// @brief Clears every resonator's state.
            void reset() noexcept;

            /// @brief Adds the modal field into left and right, read at the design's own ear gains.
            /// A design whose count is zero adds nothing and touches no state.
            /// @param design The resonators, their source gains and the two ears' gains.
            /// @param input The low half that drives the bank.
            /// @param left Receives the field at the left ear, added.
            /// @param right Receives the field at the right ear, added.
            /// @param frames The block length.
            /// @pre prepare() has run; frames beyond max_block_frames are rendered in further passes.
            void process(const ModalDesign& design, const float* input, float* left, float* right,
                         std::uint32_t frames) noexcept;

            /// @brief Adds the modal field into each read-out's block. The design's sections and
            /// source gains drive the resonators; its ear gains are not read.
            /// @param design The resonators and their source gains.
            /// @param input The low half that drives the bank.
            /// @param read_outs The receiver points, at most K_MAX_READ_OUTS.
            /// @param count The number of read-outs; those past K_MAX_READ_OUTS are ignored.
            /// @param frames The block length.
            /// @pre prepare() has run; no output aliases the input or another output.
            void process(const ModalDesign& design, const float* input, const ReadOut* read_outs,
                         std::uint32_t count, std::uint32_t frames) noexcept;

        private:
            /// @brief Renders one run of at most max_block_frames_ frames into every read-out,
            /// each output offset by the frames already rendered.
            void render_block(const ModalDesign& design, const float* input,
                              const ReadOut* read_outs, std::uint32_t count, std::uint32_t offset,
                              std::uint32_t frames) noexcept;

            /// @brief Stores the first transposed-direct-form state of every mode, K_MAX_MODES long.
            std::vector<double> state_first_;

            /// @brief Stores the second transposed-direct-form state of every mode, K_MAX_MODES long.
            std::vector<double> state_second_;

            /// @brief Stores one group's resonator outputs per frame and lane.
            std::vector<double> lane_output_;

            /// @brief Accumulates each read-out per frame and lane, K_MAX_READ_OUTS blocks of
            /// max_block_frames_ * K_LANES.
            std::vector<double> partial_;

            /// @brief Bounds the frames one render_block() call handles.
            std::uint32_t max_block_frames_ = 0;
    };
} // namespace SushiDSP::room
