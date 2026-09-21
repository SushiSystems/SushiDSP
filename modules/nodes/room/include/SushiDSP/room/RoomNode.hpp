/**************************************************************************/
/* RoomNode.hpp                                                           */
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
 * @file RoomNode.hpp
 * @brief The room node: one cabinet in one shoebox, the standing waves, the taps, the tail, the ears and the swap.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

#include "SushiDSP/core/IAudioNode.hpp"
#include "SushiDSP/core/NodeDescriptor.hpp"
#include "SushiDSP/core/ParameterTable.hpp"
#include "SushiDSP/core/RtSwapSlot.hpp"
#include "SushiDSP/math/SmoothedParameter.hpp"
#include "SushiDSP/room/BinauralDecoder.hpp"
#include "SushiDSP/room/CrossoverPair.hpp"
#include "SushiDSP/room/EarlyReflectionRenderer.hpp"
#include "SushiDSP/room/EarlyTapSet.hpp"
#include "SushiDSP/room/HrtfSet.hpp"
#include "SushiDSP/room/IRoomHost.hpp"
#include "SushiDSP/room/LateReverb.hpp"
#include "SushiDSP/room/ModalBank.hpp"
#include "SushiDSP/room/ModalDesign.hpp"
#include "SushiDSP/room/RoomRenderSet.hpp"
#include "SushiDSP/room/RoomScene.hpp"

namespace SushiDSP::room
{
    /// @brief One cabinet in one room, heard from one spot: PortLayout{1, 2}. Owns the seam's
    /// crossover, the modal banks, the early renderer, the direct path, the tail, the decoder
    /// slot and the render-set slot; every set is built on the control thread for the listener
    /// the parameter names and crossfaded in on the audio thread. The ears hear the direct sound
    /// through its HRIR pair, the reflections through the decoder and the tail through the
    /// diffuse-field pair; the microphone hears channel 0 of its taps, the tail read once and
    /// the modal field at one point. Spec §1, §5.2, §8, §9 and §9a.
    class RoomNode final : public IAudioNode, public IRoomHost
    {
    public:
        /// @brief Counts the frames a newly acquired set fades in over, raised cosine.
        static constexpr std::uint32_t kCrossfadeFrames = 512;

        /// @brief Puts every parameter at its descriptor default and the default scene in place.
        RoomNode();

        /// @brief Sizes the input rings, the tail's lines, the modal banks, the alignment delays
        /// and the block buffers, builds the spherical-head fallback, publishes its decoder and a
        /// first set for the scene in place.
        void prepare(const PrepareInfo& info) override;

        /// @brief Renders one block: one acquire per slot, the split at the seam, the early
        /// taps, the direct path, the tail, the standing waves, the ears or the microphone, the
        /// mix and the level.
        void process(ProcessContext& context) override;

        /// @brief Returns mono in, two channels out.
        PortLayout ports() const override;

        /// @brief Reads a parameter by id; an unknown id reads 0.
        float get_parameter(const char* id) const override;

        /// @brief Writes a parameter by id. Control thread only. A changed listener or direct
        /// delay rebuilds the set through the path set_scene uses; the audio thread applies
        /// either choice only through the next published set.
        void set_parameter_for_control_thread(const char* id, float value) override;

        /// @brief Writes a parameter by descriptor index; stores a target and nothing else, so
        /// a listener or direct delay written here waits for the next publish.
        void set_parameter_for_audio_thread(std::uint16_t index, float value) override;

        /// @brief This node's parameters, one row each, feeding the descriptor and both
        /// dispatches. Declared on the class rather than at namespace scope so a row's
        /// lambdas can reach the private field a knob actually lives in.
        /// @return The table, built once on first use.
        static const ParameterTable<RoomNode, 4>& parameters();

        /// @brief Silences the rings, every state bank, the tail, the decoder, the crossover,
        /// the alignment delays and the crossfade.
        void reset() override;

        /// @brief Returns the ears' latency: one decoder partition, plus the direct path's
        /// propagation delay when direct_delay is physical; the microphone's is 0. Spec §9.4.
        std::uint32_t latency_samples() const override;

        /// @brief Returns "sushidsp.room".
        const char* type_id() const override;

        bool       set_scene(const RoomScene& scene, std::string& reason_out) override;
        RoomScene  scene() const override;
        bool       set_hrtf(HrtfSet set) override;
        HrtfSource hrtf_source() const override;
        double     schroeder_frequency_hz() const override;
        void       reclaim_rooms() override;

        /// @brief Copies a set's taps into a bank: an ears set's tap 0, the (0, 0, 0) image the
        /// HRIR pair renders, goes into direct and the rest into reflections; a microphone set's
        /// taps all go into reflections and direct is left as it was. Audio thread.
        /// @pre The taps are sorted by arrival, so tap 0 is the direct image whenever count > 0.
        /// @return True when direct was written.
        static bool take_direct_image(const RoomRenderSet& set, EarlyTapSet& reflections,
                                      EarlyTap& direct) noexcept;

        /// @brief Moves an ears set's whole render earlier by its direct propagation delay, so
        /// the direct sound lands at the decoder's partition and the reflections and the tail
        /// keep their spacing to it: every tap but the first is floored at one frame, the tail's
        /// input delay at zero. A microphone set is left as it was. Control thread. Spec §9.4.
        static void compensate_direct_delay(RoomRenderSet& set) noexcept;

        /// @brief Returns true when two designs hold the same resonators: the same count and
        /// every paired section equal bit for bit, as one room designs them whatever the poses.
        static bool same_resonators(const ModalDesign& retiring,
                                    const ModalDesign& arriving) noexcept;

    private:
        /// @brief Counts the channels the early renderer accumulates into.
        static constexpr std::uint32_t kChannels = EarlyReflectionRenderer::kOutputChannels;

        /// @brief Counts the state banks, one per side of a crossfade.
        static constexpr std::uint32_t kBanks = EarlyReflectionRenderer::kBankCount;

        /// @brief Counts the ears the seam is split for.
        static constexpr std::uint32_t kEars = 2;

        /// @brief Counts the taps of one direct-path HRIR and of one diffuse-field filter.
        static constexpr std::uint32_t kFirTaps = HrtfSet::kTapCount;

    public:
        /// @brief Holds the frames of one block a crossfade covers, how far the fade had run
        /// when the block began, and the lag the fade is read at by a path that blends at the
        /// output: one partition when the ears take part, so the paths the decoder or the
        /// alignment delay hold back, which read the window unlagged, come out in step.
        struct FadeWindow
        {
            /// @brief Counts the frames that ran since the set was adopted, before this block.
            std::uint32_t done;

            /// @brief Counts the frames of this block still inside the fade.
            std::uint32_t fading;

            /// @brief Stores the frames a path blending at the output reads the fade behind.
            std::uint32_t lag;
        };

    private:

        /// @brief Convolves a block with kFirTaps coefficients over a history it keeps; the
        /// coefficients are copied in and the history survives a coefficient change. Audio thread.
        class FirFilter
        {
        public:
            /// @brief Sizes the history for the longest block.
            void prepare(std::uint32_t max_block_frames);

            /// @brief Copies kFirTaps coefficients in.
            void load(const float* taps) noexcept;

            /// @brief Zeroes the history.
            void reset() noexcept;

            /// @brief Copies another filter's history, not its coefficients.
            void carry_from(const FirFilter& other) noexcept;

            /// @brief Writes the convolution of one block into output.
            /// @pre frames <= max_block_frames; input and output do not alias.
            void process(const float* input, float* output, std::uint32_t frames) noexcept;

        private:
            /// @brief Stores the coefficients, tap 0 first.
            std::array<float, kFirTaps> taps_{};

            /// @brief Stores kFirTaps - 1 frames of history followed by room for one block.
            std::vector<float> history_;
        };

        /// @brief Delays a block by a fixed count of frames. Audio thread.
        class FrameDelay
        {
        public:
            /// @brief Sizes the line for the delay and the longest block.
            void prepare(std::uint32_t delay_frames, std::uint32_t max_block_frames);

            /// @brief Zeroes the line.
            void reset() noexcept;

            /// @brief Writes one block delayed by the prepared count into output.
            /// @pre frames <= max_block_frames; input and output may alias.
            void process(const float* input, float* output, std::uint32_t frames) noexcept;

        private:
            /// @brief Stores the delayed frames followed by room for one block.
            std::vector<float> line_;

            /// @brief Stores the prepared delay.
            std::uint32_t delay_frames_ = 0;
        };

        /// @brief Reads fractional delays out of one input ring. Audio thread.
        class DelayRing
        {
        public:
            /// @brief Sizes the ring for the longest read and the longest block.
            void prepare(std::uint32_t max_delay_frames, std::uint32_t max_block_frames);

            /// @brief Zeroes the ring.
            void reset() noexcept;

            /// @brief Writes one block. Call once per block, before any read.
            void write(const float* input, std::uint32_t frames) noexcept;

            /// @brief Reads one block at a fractional delay behind the block last written,
            /// through the early renderer's four-point Lagrange kernel, scaled by gain.
            /// @pre frames matches the block last written; delay_frames is clamped to
            /// [1, max_delay_frames].
            void read(double delay_frames, float gain, float* output,
                      std::uint32_t frames) const noexcept;

        private:
            /// @brief Stores the ring, a power of two long.
            std::vector<float> ring_;

            /// @brief Masks an index into the ring.
            std::uint32_t mask_ = 0;

            /// @brief Indexes the ring frame the last written block started at.
            std::uint32_t block_start_ = 0;

            /// @brief Indexes the ring frame the next write lands on.
            std::uint32_t write_head_ = 0;

            /// @brief Bounds the delay a read may ask for.
            std::uint32_t max_delay_frames_ = 0;
        };

        /// @brief Renders the ears' direct sound from one bank's coefficients: the node's ring
        /// read at the alignment delay plus the tap's, the far ear also at the ITD, the tap's
        /// fitted filter and gain, and the HRIR pair. Two banks, so a swap crossfades. Audio
        /// thread.
        class DirectPath
        {
        public:
            /// @brief Sizes both banks' histories and records the alignment delay.
            void prepare(std::uint32_t alignment_frames, std::uint32_t max_block_frames);

            /// @brief Zeroes both banks' state and deactivates both.
            void reset() noexcept;

            /// @brief Loads one bank with a tap and a set's HRIR pair and ITD, and activates it.
            void load_bank(std::uint32_t bank, const EarlyTap& tap,
                           const RoomRenderSet& set) noexcept;

            /// @brief Zeroes one bank's state and deactivates it.
            void clear_bank(std::uint32_t bank) noexcept;

            /// @brief Copies one bank's filter and history state into another.
            void carry_bank(std::uint32_t from, std::uint32_t to) noexcept;

            /// @brief Returns the tap one bank was loaded with.
            const EarlyTap& tap(std::uint32_t bank) const noexcept;

            /// @brief Reports whether one bank renders anything.
            bool active(std::uint32_t bank) const noexcept;

            /// @brief Writes one bank's direct sound to both ears, read out of the ring the
            /// block was written to; an inactive bank writes zeros.
            void render(const DelayRing& ring, std::uint32_t bank, float* left, float* right,
                        std::uint32_t frames) noexcept;

        private:
            /// @brief Holds one ear's chain in one bank: the fitted filter's state and the HRIR.
            struct Ear
            {
                /// @brief Stores the two sections' histories, in the early renderer's layout.
                std::array<double, 8> filter_state;

                /// @brief Convolves with the ear's HRIR.
                FirFilter hrir;

                /// @brief Stores the read delay in frames past the tap's, the ITD on the far ear.
                double extra_delay;
            };

            /// @brief Holds one bank: the tap, the two ears and whether it renders.
            struct Bank
            {
                /// @brief Stores the tap, its delay counted from the alignment delay.
                EarlyTap tap;

                /// @brief Stores the left and right chains.
                std::array<Ear, kEars> ears;

                /// @brief Records that the bank holds an ears set's direct path.
                bool active;
            };

            /// @brief Reads, filters and convolves one ear of one bank into output.
            void render_ear(const DelayRing& ring, Bank& bank, std::uint32_t ear, float* output,
                            std::uint32_t frames) noexcept;

            /// @brief Stores the two banks.
            std::array<Bank, kBanks> banks_{};

            /// @brief Stores one ear's block between the read and the convolution.
            std::vector<float> scratch_;

            /// @brief Stores the frames every read is delayed by, the decoder's partition.
            std::uint32_t alignment_frames_ = 0;
        };

        /// @brief Returns the listener the parameter names.
        ListenerKind listener_kind() const noexcept;

        /// @brief Reports whether the direct_delay parameter names physical.
        bool direct_delay_physical() const noexcept;

        /// @brief Builds a set for the scene, HRTF, listener and direct delay in place,
        /// publishes it and records the frequency the set splits at and the latency it carries.
        /// @return False and fills reason_out when the build refuses the scene or the set.
        bool publish(std::string& reason_out);

        /// @brief Builds a decoder from the HRTF in place and publishes it.
        /// @throw std::runtime_error when the set carries no third-order decoder.
        void publish_decoder();

        /// @brief Copies a newly acquired set's taps into the spare bank, zeroes that bank's
        /// state, carries the active bank's state into every tap the arriving set keeps, loads
        /// the direct path's bank the same way, hands the modal design to adopt_modal, places
        /// the crossover, points the tail's outputs and the diffuse pair for the set's listener
        /// and starts the fade; the first set ever is copied in from zero state with no fade.
        /// Taps pair by index: an arriving tap past the retiring count starts from zero, a
        /// retiring tap past the arriving count is dropped. Spec §5.2.
        void adopt(const RoomRenderSet& set) noexcept;

        /// @brief Adopts a set's modal design under the match rule: resonators identical to the
        /// active bank's keep that bank's state and ramp its gains over the fade; resonators
        /// that differ start the spare bank from zero and blend the two over the fade.
        void adopt_modal(const ModalDesign& design) noexcept;

        /// @brief Renders the active bank, and the retiring bank while a fade runs, each into
        /// its own accumulation set.
        void render_early(std::uint32_t frames, const FadeWindow& window) noexcept;

        /// @brief Weights each rendered bank by its side of the fade and routes it by its
        /// listener: an ears bank's sixteen channels go through the decoder into
        /// decoded_left_ and decoded_right_, weighted unlagged for the partition the decoder
        /// holds them, a microphone bank's channel 0 into early_mono_, weighted at the lag.
        void route_early(BinauralDecoder* decoder, std::uint32_t frames,
                         const FadeWindow& window) noexcept;

        /// @brief Decodes sixteen channels into decoded_left_ and decoded_right_, resetting
        /// the decoder first when it sat idle; a missing decoder writes zeros.
        void decode(BinauralDecoder* decoder, const float* const* channels,
                    std::uint32_t frames) noexcept;

        /// @brief Renders the active bank's direct sound, blended with the retiring bank's
        /// under the fade, into direct_left_ and direct_right_.
        void render_direct(std::uint32_t frames, const FadeWindow& window) noexcept;

        /// @brief Reads the tail's input out of the ring at the set's delay to t_c, blended
        /// with the read at the retiring set's delay under the fade, into tail_input_; the
        /// tail's own input delay is zero, so a compensated delay never jumps. Spec §9.4.
        void read_tail_input(std::uint32_t frames, const FadeWindow& window) noexcept;

        /// @brief Holds what the tail takes from a set: its design with the input delay moved
        /// to the ring, the right ear's routing and the diffuse pair.
        struct TailAdoption
        {
            /// @brief Stores the design, its delay to t_c zeroed.
            FdnDesign design;

            /// @brief Records that the right ear reads the coherence pair, not the mono tail.
            bool two_ears;

            /// @brief Stores the left ear's diffuse-field average.
            std::array<float, kFirTaps> diffuse_left;

            /// @brief Stores the right ear's diffuse-field average.
            std::array<float, kFirTaps> diffuse_right;
        };

        /// @brief Copies what the tail takes from a set into pending_tail_; it is applied at
        /// once when the fade has no lag and at the block the lagged fade begins otherwise,
        /// so nothing at the output moves before the fade does.
        void stage_tail(const RoomRenderSet& set) noexcept;

        /// @brief Applies pending_tail_ once the fade has run its lag.
        void apply_tail_when_due(const FadeWindow& window) noexcept;

        /// @brief Splits the input at the seam into modal_low_ and geometric_high_; a bypassed
        /// crossover passes everything to the high half.
        void split_input(const float* input, std::uint32_t frames) noexcept;

        /// @brief Runs the low half through the active modal bank, blending the retiring bank
        /// or ramping the gains under the fade, into modal_left_ and modal_right_; the fade is
        /// read unlagged, the ears' field waiting the alignment delay after it.
        void render_modal(std::uint32_t frames, const FadeWindow& window) noexcept;

        /// @brief Runs the active bank over one frame with its gains at the fade's weight
        /// between the retiring and the arriving design.
        void render_modal_ramp(std::uint32_t frame, float weight) noexcept;

        /// @brief Passes the tail through the diffuse-field pair and the tail and the modal
        /// field through the alignment delay, the ears' presentations of both.
        void align_ears(std::uint32_t frames) noexcept;

        /// @brief Sums each ear into ear_: the ears' presentation, the microphone's, or the two
        /// weighted by the banks' sides of the fade when a fade crosses listeners.
        void join_ears(std::uint32_t frames, const FadeWindow& window) noexcept;

        /// @brief Writes the smoothed dry-wet mix at the smoothed level to both outputs.
        void write_outputs(const float* input, float* const* outputs,
                           std::uint32_t frames) noexcept;

        /// @brief Zeroes one accumulation set over the block.
        void clear_accumulation(std::uint32_t set, std::uint32_t frames) noexcept;

        /// @brief Stores the rate prepare() ran at, in hertz.
        double sample_rate_ = 48000.0;

        /// @brief Bounds the block length prepare() sized the buffers for.
        std::uint32_t max_block_ = 0;

        /// @brief Stores the decoder's partition, the frames every other ears path waits.
        std::uint32_t alignment_frames_ = 0;

        /// @brief Records that prepare() has run.
        bool prepared_ = false;

        /// @brief Stores the scene in place; control thread.
        RoomScene scene_;

        /// @brief Stores the HRTF set in place; control thread.
        HrtfSet hrtf_;

        /// @brief Names where hrtf_ came from.
        HrtfSource hrtf_source_ = HrtfSource::SphericalHead;

        /// @brief Stores the frequency the published set splits at, in hertz: the solver's
        /// crossover while the modal layer runs, Eyring's Schroeder frequency otherwise.
        double schroeder_hz_ = 0.0;

        /// @brief Smooths the dry-wet mix, 0 dry to 1 wet.
        SmoothedParameter mix_{ 1.0f };

        /// @brief Smooths the output level as a linear gain.
        SmoothedParameter level_{ 1.0f };

        /// @brief Stores the level parameter in decibels, what get_parameter reads back.
        float level_db_ = 0.0f;

        /// @brief Stores the listener choice, 0 ears and 1 microphone; written on either
        /// thread, read at publish.
        std::atomic<float> listener_{ 0.0f };

        /// @brief Names the listener the set published last was built for; control thread.
        ListenerKind published_listener_ = ListenerKind::Ears;

        /// @brief Stores the direct-delay choice, 0 compensated and 1 physical; written on
        /// either thread, read at publish.
        std::atomic<float> direct_delay_{ 0.0f };

        /// @brief Records that the set published last was built physical; control thread.
        bool published_physical_ = false;

        /// @brief Stores the propagation delay the set published last adds to the latency in
        /// frames: round(d / c * fs) when physical, zero otherwise; control thread.
        std::uint32_t published_direct_delay_frames_ = 0;

        /// @brief Carries each published set to the audio thread.
        RtSwapSlot<RoomRenderSet> slot_;

        /// @brief Carries each published decoder to the audio thread.
        RtSwapSlot<BinauralDecoder> decoder_slot_;

        /// @brief Identifies the set adopted last; compared, never dereferenced past its block.
        const RoomRenderSet* adopted_ = nullptr;

        /// @brief Records that the decoder ran last block, so its history is current.
        bool decoder_fed_ = false;

        /// @brief Reads the taps out of its own input ring.
        EarlyReflectionRenderer early_;

        /// @brief Holds the geometric half of the input for the direct path and the tail.
        DelayRing geometric_ring_;

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

        /// @brief Stores the tail's input for one block.
        std::vector<float> tail_input_;

        /// @brief Stores the retiring delay's read while a fade runs.
        std::vector<float> tail_input_retiring_;

        /// @brief Stores the tail adoption waiting for the lagged fade to begin.
        TailAdoption pending_tail_{};

        /// @brief Records that pending_tail_ has not been applied yet.
        bool tail_pending_ = false;

        /// @brief Passes the ears' tail through the diffuse-field average, one filter per ear.
        std::array<FirFilter, kEars> diffuse_{};

        /// @brief Delays the ears' tail to the decoder's partition, one line per ear.
        std::array<FrameDelay, kEars> tail_delay_{};

        /// @brief Delays the ears' modal field to the decoder's partition, one line per ear.
        std::array<FrameDelay, kEars> modal_delay_{};

        /// @brief Stores the taps each state bank renders; the retiring set is read from here,
        /// never from a slot pointer, so a reclaim can free it at any time.
        std::array<EarlyTapSet, kBanks> bank_taps_{};

        /// @brief Names the listener each bank's set was built for.
        std::array<ListenerKind, kBanks> bank_listener_{};

        /// @brief Indexes the bank rendering the set in place.
        std::uint32_t active_bank_ = 0;

        /// @brief Counts the crossfade frames left; zero when no fade runs.
        std::uint32_t fade_remaining_ = 0;

        /// @brief Counts the frames the running fade spans: kCrossfadeFrames plus its lag.
        std::uint32_t fade_total_ = 0;

        /// @brief Stores the lag the running fade is read at by the paths that blend at the
        /// output: the alignment delay when either bank is the ears, zero otherwise.
        std::uint32_t fade_lag_ = 0;

        /// @brief Stores kBanks sets of kChannels accumulation buffers of max_block_ frames.
        std::vector<float> accumulation_;

        /// @brief Points at each accumulation set's kChannels buffers.
        std::array<std::array<float*, kChannels>, kBanks> accumulation_pointers_{};

        /// @brief Stores the tail's block: the microphone's, or the ears' left.
        std::vector<float> tail_;

        /// @brief Stores the ears' right tail block.
        std::vector<float> tail_right_;

        /// @brief Points at the tail buffer each ear reads; both at tail_ for the microphone.
        std::array<float*, kEars> tail_outputs_{};

        /// @brief Stores the ears' tail, through the diffuse pair and the alignment delay.
        std::array<std::vector<float>, kEars> tail_ears_{};

        /// @brief Splits the input at the seam: the low half feeds the modal bank, the high half
        /// the early renderer, the direct path and the tail.
        CrossoverPair input_crossover_;

        /// @brief Renders the standing waves, one bank per side of a crossfade.
        std::array<ModalBank, kBanks> modal_banks_;

        /// @brief Stores the design each modal bank renders; copied in like bank_taps_.
        std::array<ModalDesign, kBanks> bank_modes_{};

        /// @brief Stores the gains the active bank ramps from, the design rendered last.
        ModalDesign modal_from_{};

        /// @brief Stores the active bank's resonators with the gains at the ramp's step.
        ModalDesign modal_blend_{};

        /// @brief Indexes the modal bank rendering the set in place.
        std::uint32_t active_modal_bank_ = 0;

        /// @brief Records that the retiring modal bank blends out under the running fade.
        bool modal_fading_ = false;

        /// @brief Records that the active modal bank's gains ramp under the running fade.
        bool modal_ramping_ = false;

        /// @brief Stores the input's low half, the modal bank's feed.
        std::vector<float> modal_low_;

        /// @brief Stores the input's high half, the geometric layers' feed.
        std::vector<float> geometric_high_;

        /// @brief Stores the modal field at the left ear.
        std::vector<float> modal_left_;

        /// @brief Stores the modal field at the right ear.
        std::vector<float> modal_right_;

        /// @brief Stores the ears' modal field through the alignment delay.
        std::array<std::vector<float>, kEars> modal_ears_{};

        /// @brief Stores the retiring modal bank's left ear while it fades out.
        std::vector<float> retiring_left_;

        /// @brief Stores the retiring modal bank's right ear while it fades out.
        std::vector<float> retiring_right_;

        /// @brief Stores the ears' decoded reflections, left.
        std::vector<float> decoded_left_;

        /// @brief Stores the ears' decoded reflections, right.
        std::vector<float> decoded_right_;

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

        /// @brief Stores kEars blocks of the joined wet field, one per ear.
        std::vector<float> ear_;

        /// @brief Points at each ear's block of ear_.
        std::array<float*, kEars> ear_pointers_{};
    };

    /// @brief Returns the registration entry for "sushidsp.room".
    const NodeDescriptor& room_descriptor();
}
