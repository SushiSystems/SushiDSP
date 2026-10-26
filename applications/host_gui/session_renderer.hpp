/**************************************************************************/
/* session_renderer.hpp                                                   */
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
 *  @file session_renderer.hpp
 *  @brief Declares the one place a block of the session is rendered: every track through its rig.
 *  @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "session.hpp"

#include <SushiDSP/foundation/core/rt_counter.hpp>
#include <SushiDSP/foundation/core/rt_counter_report.hpp>

/// @brief Renders one block of a session: each track's lane, and the live input on the selected
/// track, through that track's rig and strip into the master, then the recorder, the click and
/// the transport. The device callback and the export both call it. Audio thread; allocation-free
/// after prepare().
/// @see docs/agent/2026_10_13_TRACK_RIGS_AND_PLUGIN/SPEC.md, section 4.2
class SessionRenderer
{
    public:
        /// @brief Counts the channels a rig takes and gives.
        static constexpr std::uint32_t K_CHANNELS = 2;

        /// @brief Holds what render() counts instead of logging.
        struct Counters
        {
                /// @brief Blocks longer than the prepared size, whose tail was not rendered; the
                /// peak is the longest block asked.
                SushiDSP::RtCounter cut_block;

                /// @brief Adds the counter to @p reporter under its name.
                void add_to(SushiDSP::RtCounterReporter& reporter) const
                {
                    reporter.add("cut_block", cut_block);
                }
        };

        /// @brief Returns what render() counted; read it on the control thread.
        [[nodiscard]] const Counters& counters() const noexcept;

        /// @brief Sizes the block buffers; control thread.
        /// @param max_block_frames The longest block render() will see.
        void prepare(std::uint32_t max_block_frames);

        /// @brief Renders one block into the master.
        /// @param session The session; its audio table and its timeline are read.
        /// @param live The live input, K_CHANNELS pointers of frames samples, or nullptr for
        /// silence.
        /// @param outputs K_CHANNELS writable buffers of frames samples; overwritten.
        /// @param frames The block length, at most the prepared one; a longer block is cut and
        /// counted in counters().cut_block.
        /// @pre prepare() has run.
        void render(Session& session, const float* const* live, float* const* outputs,
                    std::uint32_t frames) noexcept;

    private:
        /// @brief Returns one channel of one of the two scratch buses.
        float* channel(std::vector<float>& bus, std::uint32_t index) noexcept;

        /// @brief Holds what render() counted.
        Counters counters_;

        /// @brief Holds the interned lane name of each track, for the profiler.
        std::array<std::uint32_t, Session::K_MAX_TRACKS> track_lanes_{};

        /// @brief Bounds the block length prepare() sized the buffers for.
        std::uint32_t max_block_ = 0;

        /// @brief Stores a track's input, K_CHANNELS blocks.
        std::vector<float> input_;

        /// @brief Stores a track's rig output, K_CHANNELS blocks.
        std::vector<float> rig_out_;

        /// @brief Stores the live input's first channel as the recorder's dry leg.
        std::vector<float> dry_;
};
