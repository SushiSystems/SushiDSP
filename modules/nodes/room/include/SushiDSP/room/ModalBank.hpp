/**************************************************************************/
/* ModalBank.hpp                                                          */
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
 * @file ModalBank.hpp
 * @brief The bank of second-order resonators that renders the room's standing waves.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "SushiDSP/room/ModalDesign.hpp"

namespace SushiDSP::room
{
    /// @brief Up to kMaxModes second-order resonators, each fed by the source's mode shape and
    /// read at the two ears' mode shapes. Audio thread; 13 flops per mode per sample. Spec §9a.2.
    class ModalBank
    {
    public:
        /// @brief Counts the modes one pass of the inner loop advances side by side.
        static constexpr std::size_t kLanes = 4;

        /// @brief Sizes the state for kMaxModes and the lane scratch for one block; control
        /// thread, before the first process().
        void prepare(std::uint32_t max_block_frames);

        /// @brief Clears every resonator's state.
        void reset() noexcept;

        /// @brief Adds the modal field into left and right. A design whose count is zero adds
        /// nothing and touches no state.
        /// @pre prepare() has run; frames beyond max_block_frames are rendered in further passes.
        void process(const ModalDesign& design, const float* input,
                     float* left, float* right, std::uint32_t frames) noexcept;

    private:
        /// @brief Renders one run of at most max_block_frames_ frames into left and right.
        void render_block(const ModalDesign& design, const float* input,
                          float* left, float* right, std::uint32_t frames) noexcept;

        /// @brief Stores the first transposed-direct-form state of every mode, kMaxModes long.
        std::vector<double> state_first_;

        /// @brief Stores the second transposed-direct-form state of every mode, kMaxModes long.
        std::vector<double> state_second_;

        /// @brief Accumulates the left ear per frame and lane, max_block_frames_ * kLanes long.
        std::vector<double> partial_left_;

        /// @brief Accumulates the right ear per frame and lane, max_block_frames_ * kLanes long.
        std::vector<double> partial_right_;

        /// @brief Bounds the frames one render_block() call handles.
        std::uint32_t max_block_frames_ = 0;
    };
}
