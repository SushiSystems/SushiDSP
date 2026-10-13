/**************************************************************************/
/* environment_render_set.hpp                                             */
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
 * @file environment_render_set.hpp
 * @brief Declares the render set of a whole arrangement and the builder that solves it.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#include "SushiDSP/room/hrtf_set.hpp"
#include "SushiDSP/room/room_arrangement.hpp"
#include "SushiDSP/room/room_render_set.hpp"
#include "SushiDSP/room/room_scene.hpp"

namespace SushiDSP::Room
{
    /// @brief Holds every coefficient the audio thread reads for one arrangement: one render set
    /// per (cabinet, receiver) pair, built as one and published through one RtSwapSlot. Receiver 0
    /// of every cabinet is the listener; receiver 1 + m is microphone m. Coefficients only.
    /// @see modules/nodes/room/README.md, "The room as an environment"
    struct EnvironmentRenderSet
    {
            /// @brief Counts the cabinets in the arrangement the set was built for.
            std::uint32_t source_count = 0;

            /// @brief Counts the microphones in the arrangement the set was built for.
            std::uint32_t microphone_count = 0;

            /// @brief Stores each pair's set, indexed [cabinet][receiver]; a pair past the counts
            /// is never read.
            std::array<std::array<room::RoomRenderSet, Arrangement::K_MAX_RECEIVERS>,
                       Arrangement::K_MAX_SOURCES>
                pairs;
    };

    /// @brief Moves an ears set's whole render earlier by its direct propagation delay, so the
    /// direct sound lands at the decoder's partition and the reflections and the tail keep their
    /// spacing to it: every tap but the first is floored at one frame, the tail's delay at zero.
    /// A microphone set is left as it was. Control thread.
    /// @param set The set to shift in place.
    void compensate_direct_delay(room::RoomRenderSet& set) noexcept;

    /// @brief Builds environment render sets on the control thread and remembers what each pair
    /// was built from, so a pair whose scene did not change is copied, not solved again.
    class EnvironmentBuilder
    {
        public:
            /// @brief Builds the set for an arrangement.
            /// @param arrangement The room, already checked with Arrangement::is_valid().
            /// @param hrtf The head the listener's direct path and tail are filtered through.
            /// @param physical_delay False shifts the listener's pairs earlier by their direct delay.
            /// @param previous The set built by the last call, still alive, or nullptr.
            /// @return The set; never null.
            /// @throw std::runtime_error when a pair's scene or the HRTF set is refused.
            std::unique_ptr<EnvironmentRenderSet> build(const Arrangement& arrangement,
                                                        const room::HrtfSet& hrtf,
                                                        double sample_rate, bool physical_delay,
                                                        const EnvironmentRenderSet* previous);

            /// @brief Forgets what the pairs were built from, so the next build solves every pair.
            /// Called when the head or the sample rate changes.
            void invalidate() noexcept;

        private:
            /// @brief Holds what one pair was last built from.
            struct Key
            {
                    /// @brief Stores the pair's scene.
                    room::RoomScene scene;

                    /// @brief Records that the direct delay was kept physical.
                    bool physical = false;

                    /// @brief Records that the key describes a pair of the last set.
                    bool valid = false;
            };

            /// @brief Holds the room's part of a set and the scene it was designed from.
            struct Shell
            {
                    /// @brief Stores the scene whose room the design reads.
                    room::RoomScene scene;

                    /// @brief Stores the design.
                    room::ShellDesign design;
            };

            /// @brief Returns the room's part for a scene, from the designs kept by the last
            /// build when one matches, else designed now. The design is kept for the next build.
            const room::ShellDesign& shell_for(const room::RoomScene& scene, double sample_rate,
                                               std::vector<Shell>& kept);

            /// @brief Stores the room designs the last build read, one per distinct room.
            std::vector<Shell> shells_;

            /// @brief Stores the keys of the set built last, indexed [cabinet][receiver].
            std::array<std::array<Key, Arrangement::K_MAX_RECEIVERS>, Arrangement::K_MAX_SOURCES>
                keys_{};

            /// @brief Stores the sample rate the keys were built at, in hertz.
            double sample_rate_ = 0.0;
    };
} // namespace SushiDSP::Room
