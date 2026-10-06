/**************************************************************************/
/* RoomRenderSet.hpp                                                      */
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
 * @file RoomRenderSet.hpp
 * @brief Everything the audio thread reads for one scene, built whole on the control thread.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <array>
#include <memory>

#include "SushiDSP/room/EarlyTapSet.hpp"
#include "SushiDSP/room/FdnDesign.hpp"
#include "SushiDSP/room/HrtfSet.hpp"
#include "SushiDSP/room/ImageSourceSolver.hpp"
#include "SushiDSP/room/ModalDesign.hpp"
#include "SushiDSP/room/RoomScene.hpp"

namespace SushiDSP::room
{
    /// @brief Holds every coefficient the audio thread reads for one scene: the early taps, the
    /// tail's design, the modal bank, the direct path's HRIR pair and the diffuse-field pair.
    /// The control thread builds one whole, owns it through RtSwapSlot and frees it on reclaim;
    /// the audio thread borrows a pointer for one block and never allocates, frees, locks or
    /// throws on it. Coefficients only, never state. Spec §5.2.
    struct RoomRenderSet
    {
        /// @brief Stores the image-source taps up to the crossover time.
        EarlyTapSet early;

        /// @brief Stores the feedback delay network that takes over at the crossover time.
        FdnDesign late;

        /// @brief Stores the resonators below the Schroeder frequency.
        ModalDesign modal;

        /// @brief Stores the left ear's minimum-phase response for the direct path.
        std::array<float, HrtfSet::kTapCount> direct_left;

        /// @brief Stores the right ear's minimum-phase response for the direct path.
        std::array<float, HrtfSet::kTapCount> direct_right;

        /// @brief Stores the direct path's interaural delay in frames; positive means the right
        /// ear is later.
        float direct_itd_frames;

        /// @brief Stores round(d / c * fs), the direct path's propagation delay in frames.
        float direct_delay_frames;

        /// @brief Stores 1 / r, the direct path's gain, linear.
        float direct_gain;

        /// @brief Stores the left ear's diffuse-field average, for the tail.
        std::array<float, HrtfSet::kDiffuseTaps> diffuse_left;

        /// @brief Stores the right ear's diffuse-field average, for the tail.
        std::array<float, HrtfSet::kDiffuseTaps> diffuse_right;

        /// @brief Stores the frequency the field splits at between the modal bank and the rays,
        /// equal to modal.crossover_hz; zero when the modal layer is off.
        float crossover_hz;

        /// @brief Names the listener the taps, the tail and the direct path are encoded for.
        ListenerKind listener;
    };

    /// @brief Builds one set on the control thread: validates the scene, solves the images,
    /// designs the tail, finds the Schroeder frequency, designs the modal bank read at the
    /// listener's receiver, and takes the direct pair from the HRTF set at the path from the
    /// source to that receiver.
    /// @pre sample_rate exceeds 8 kHz and equals hrtf.sample_rate().
    /// @throw std::runtime_error when the scene is invalid, naming the violation, or when the
    /// HRTF set is empty or measured at another rate.
    std::unique_ptr<RoomRenderSet> build_render_set(const RoomScene& scene, const HrtfSet& hrtf,
                                                    double sample_rate, ListenerKind listener);
}
