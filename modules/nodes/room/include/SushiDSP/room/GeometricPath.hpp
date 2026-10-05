/****************************************************************/
/* GeometricPath.hpp                                            */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file GeometricPath.hpp
 * @brief The crossover's high half: the direct sound, the image-source taps and the tail.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "SushiDSP/room/DelayRing.hpp"
#include "SushiDSP/room/DirectPath.hpp"
#include "SushiDSP/room/EarlyReflectionRenderer.hpp"
#include "SushiDSP/room/EarlyTapSet.hpp"
#include "SushiDSP/room/FadeWindow.hpp"
#include "SushiDSP/room/FdnDesign.hpp"
#include "SushiDSP/room/ImageSourceSolver.hpp"
#include "SushiDSP/room/LateReverb.hpp"
#include "SushiDSP/room/RoomRenderSet.hpp"

namespace SushiDSP::room
{
    /// @brief Renders the rays from the input's high half: the direct sound at the ears, the
    /// image-source taps encoded to sixteen channels, and the tail the taps hand over to at the
    /// crossover time. Holds two state banks, one per side of a crossfade, and weights each by
    /// its side of the window before the ears or the microphone read it. Spec §5.2, §8, §9.4.
    class GeometricPath
    {
    public:
        /// @brief Counts the channels the early renderer accumulates into.
        static constexpr std::uint32_t kChannels = EarlyReflectionRenderer::kOutputChannels;

        /// @brief Counts the state banks, one per side of a crossfade.
        static constexpr std::uint32_t kBanks = EarlyReflectionRenderer::kBankCount;

        /// @brief Sizes the two rings, the tail's lines, the direct path and every block buffer.
        void prepare(double sample_rate, std::uint32_t alignment_frames,
                     std::uint32_t max_block_frames);

        /// @brief Zeroes the rings, the direct path and the tail.
        void reset() noexcept;

        /// @brief Returns the buffer the input crossover writes the high half into.
        float* high_buffer() noexcept;

        /// @brief Writes the high half into the early renderer's ring and its own.
        void write_input(std::uint32_t frames) noexcept;

        /// @brief Copies what the tail takes from a set aside until the fade is due to move it.
        /// @pre first says that no set has been adopted yet, so the read delay starts in place.
        void stage_tail(const RoomRenderSet& set, bool first) noexcept;

        /// @brief Puts the staged tail design and routing in place.
        void apply_pending() noexcept;

        /// @brief Takes the first set ever: the active bank is loaded from zero state.
        void adopt_first(const RoomRenderSet& set) noexcept;

        /// @brief Copies a set's taps into the spare bank, carries the active bank's state into
        /// every tap the arriving set keeps, loads the direct path's bank the same way and makes
        /// the spare bank active. Taps pair by index: an arriving tap past the retiring count
        /// starts from zero, a retiring tap past the arriving count is dropped. Spec §5.2.
        /// @return True when either side of the fade is an ears set, so the fade needs the lag.
        bool adopt(const RoomRenderSet& set) noexcept;

        /// @brief Renders the active bank, and the retiring bank while a fade runs, each into
        /// its own accumulation set.
        void render_early(std::uint32_t frames, const FadeWindow& window) noexcept;

        /// @brief Reads the tail's input out of the ring at the set's delay to t_c, blended with
        /// the read at the retiring set's delay under the fade, and runs the tail over it.
        void render_tail(std::uint32_t frames, const FadeWindow& window) noexcept;

        /// @brief Renders the active bank's direct sound, blended with the retiring bank's
        /// under the fade.
        void render_direct(std::uint32_t frames, const FadeWindow& window) noexcept;

        /// @brief Weights each rendered bank by its side of the fade and routes it by its
        /// listener: an ears bank's sixteen channels are handed back for the decoder, weighted
        /// unlagged for the partition the decoder holds them, a microphone bank's channel 0 goes
        /// into the mono block, weighted at the lag.
        /// @return The sixteen channels to decode, or nullptr when neither bank is an ears set.
        const float* const* route_early(std::uint32_t frames, const FadeWindow& window) noexcept;

        /// @brief Reports whether the bank in place was built for the ears.
        bool active_is_ears() const noexcept;

        /// @brief Reports whether the two banks were built for different listeners.
        bool listeners_differ() const noexcept;

        /// @brief Reports whether either bank was built for the microphone.
        bool any_microphone() const noexcept;

        /// @brief Returns the direct sound at one ear, blended across the fade.
        const float* direct(std::uint32_t ear) const noexcept;

        /// @brief Returns the tail block one ear reads; both ears read one block for the
        /// microphone.
        const float* tail(std::uint32_t ear) const noexcept;

        /// @brief Returns the microphone's channel 0, weighted by its side of the fade.
        const float* early_mono() const noexcept;

        /// @brief Copies a set's taps into a bank: an ears set's tap 0, the (0, 0, 0) image the
        /// HRIR pair renders, goes into direct and the rest into reflections; a microphone set's
        /// taps all go into reflections and direct is left as it was. Audio thread.
        /// @pre The taps are sorted by arrival, so tap 0 is the direct image whenever count > 0.
        /// @return True when direct was written.
        static bool take_direct_image(const RoomRenderSet& set, EarlyTapSet& reflections,
                                      EarlyTap& direct) noexcept;

    private:
        /// @brief Holds what the tail takes from a set: its design with the input delay moved
        /// to the ring, and the right ear's routing.
        struct TailAdoption
        {
            /// @brief Stores the design, its delay to t_c zeroed.
            FdnDesign design;

            /// @brief Records that the right ear reads the coherence pair, not the mono tail.
            bool two_ears;
        };

        /// @brief Reads the tail's input out of the ring, blended across the fade.
        void read_tail_input(std::uint32_t frames, const FadeWindow& window) noexcept;

        /// @brief Zeroes one accumulation set over the block.
        void clear_accumulation(std::uint32_t set, std::uint32_t frames) noexcept;

        /// @brief Reads the taps out of its own input ring.
        EarlyReflectionRenderer early_;

        /// @brief Holds the high half for the direct path and the tail.
        DelayRing ring_;

        /// @brief Renders the ears' direct sound.
        DirectPath direct_;

        /// @brief Renders the tail.
        LateReverb late_;

        /// @brief Stores the adopted set's tail design with its input delay moved to the ring.
        FdnDesign tail_design_{};

        /// @brief Stores the delay to t_c the tail's input is read at, in frames.
        float tail_delay_to_ = 0.0f;

        /// @brief Stores the retiring set's delay to t_c, read under the fade.
        float tail_delay_from_ = 0.0f;

        /// @brief Stores the tail adoption waiting for the lagged fade to begin.
        TailAdoption pending_{};

        /// @brief Stores the taps each state bank renders; the retiring set is read from here,
        /// never from a slot pointer, so a reclaim can free it at any time.
        std::array<EarlyTapSet, kBanks> bank_taps_{};

        /// @brief Names the listener each bank's set was built for.
        std::array<ListenerKind, kBanks> bank_listener_{};

        /// @brief Indexes the bank rendering the set in place.
        std::uint32_t active_bank_ = 0;

        /// @brief Stores kBanks sets of kChannels accumulation buffers of one block each.
        std::vector<float> accumulation_;

        /// @brief Points at each accumulation set's kChannels buffers.
        std::array<std::array<float*, kChannels>, kBanks> accumulation_pointers_{};

        /// @brief Stores the input's high half, the geometric layers' feed.
        std::vector<float> high_;

        /// @brief Stores the tail's input for one block.
        std::vector<float> tail_input_;

        /// @brief Stores the retiring delay's read while a fade runs.
        std::vector<float> tail_input_retiring_;

        /// @brief Stores the tail's block: the microphone's, or the ears' left.
        std::vector<float> tail_;

        /// @brief Stores the ears' right tail block.
        std::vector<float> tail_right_;

        /// @brief Points at the tail buffer each ear reads; both at tail_ for the microphone.
        std::array<float*, kEars> tail_outputs_{};

        /// @brief Stores the microphone's channel 0, weighted by its side of the fade.
        std::vector<float> early_mono_;

        /// @brief Stores the ears' direct sound, left, blended across the fade.
        std::vector<float> direct_left_;

        /// @brief Stores the ears' direct sound, right, blended across the fade.
        std::vector<float> direct_right_;

        /// @brief Stores the retiring bank's direct sound, left, while it fades out.
        std::vector<float> retiring_direct_left_;

        /// @brief Stores the retiring bank's direct sound, right, while it fades out.
        std::vector<float> retiring_direct_right_;
    };
}
