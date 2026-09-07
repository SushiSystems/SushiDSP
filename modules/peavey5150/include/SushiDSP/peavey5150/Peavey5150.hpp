/**************************************************************************/
/* Peavey5150.hpp                                                         */
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

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "SushiDSP/core/Calibration.hpp"
#include "SushiDSP/core/IAudioNode.hpp"
#include "SushiDSP/core/ICabinetHost.hpp"
#include "SushiDSP/core/NodeDescriptor.hpp"
#include "SushiDSP/core/RtSwapSlot.hpp"
#include "SushiDSP/io/Ir.hpp"
#include "SushiDSP/peavey5150/Peavey5150ToneStack.hpp"
#include "SushiDSP/circuits/stages/OutputTransformerStage.hpp"
#include "SushiDSP/circuits/stages/PentodeStage.hpp"
#include "SushiDSP/circuits/stages/PowerSupplySagStage.hpp"
#include "SushiDSP/circuits/stages/ResonanceControl.hpp"
#include "SushiDSP/circuits/stages/TriodeStage.hpp"
#include "SushiDSP/math/Biquad.hpp"
#include "SushiDSP/math/Oversampler.hpp"
#include "SushiDSP/math/SmoothedParameter.hpp"
#include "SushiDSP/math/StreamingConvolution.hpp"

namespace SushiDSP
{
    /// @brief The Peavey 5150 block-letter lead channel: six preamp triodes, the shared FMV deck,
    /// the post-gain node's follower and driver, a long-tailed pair, a 6L6GC quad, the output
    /// transformer, power sag, the feedback network's resonance and presence shelves and an
    /// optional cabinet convolution, followed by a plain full-scale clamp. Component values come
    /// from docs/reference/research/2026-09-06-peavey-5150-design-dossier.md and every departure
    /// from it is listed in modules/peavey5150/README.md. Exposes peavey5150_descriptor().
    class Peavey5150 : public IAudioNode, public ICabinetHost
    {
    public:
        /// @param cabinet_impulse_response Cabinet IR samples; empty means no cabinet stage.
        explicit Peavey5150(std::vector<float> cabinet_impulse_response = {});

        /// @brief Sizes every scratch buffer and rebuilds every sample-rate-dependent filter.
        /// @param info Sample rate, max block size, and channel count for this node's lifetime.
        void prepare(const PrepareInfo& info) override;

        /// @brief Runs one block through the full amp chain. Allocation-free.
        /// @param context The per-block input/output buffers and frame count.
        void process(ProcessContext& context) override;

        /// @brief Returns this node's fixed input/output channel counts.
        /// @return Mono in, mono out.
        PortLayout ports() const override;

        /// @brief Clears every stage's state for a click-free restart. Control thread only: it
        /// reaches the cabinet through the slot's publisher-side accessor, so calling it from the
        /// audio thread would race reclaim().
        void reset() override;

        /// @brief Reports the delay this node adds from input to output.
        /// @return The single oversampled region's latency plus, when a cabinet is loaded, its
        /// one-partition convolution latency.
        std::uint32_t latency_samples() const override;

        /// @brief This amp's registered type id.
        /// @return "sushidsp.peavey5150".
        const char* type_id() const override;

        /// @brief Installs a new cabinet impulse response. Control thread only: the convolver is
        /// built here in full and handed to the audio thread as one atomic pointer swap.
        /// @param impulse_response The IR and the rate it was captured at; its sample_rate must
        /// equal the rate this node was prepared at.
        /// @return True if the cabinet was installed; false if the IR is empty, if the rates
        /// disagree, or if prepare() has not run yet, in which case nothing is installed.
        bool set_cabinet(Ir impulse_response) override;

        /// @brief Frees every cabinet the audio thread has provably moved past. Control thread
        /// only; call periodically after any set_cabinet().
        void reclaim_cabinets() override;

        /// @brief Whether a cabinet convolution is currently in the signal path.
        /// @return True once a cabinet has been installed by the constructor or set_cabinet().
        bool has_cabinet() const override;

        /// @brief Sets VR2, the lead channel's PRE GAIN.
        /// @param normalized Pot position in [0, 1]; smoothed toward on the audio thread.
        void set_pre_gain(float normalized);

        /// @brief Sets VR3, the LOW control.
        /// @param normalized Pot position in [0, 1]; 0.5 is noon.
        void set_low(float normalized);

        /// @brief Sets VR4, the MID control.
        /// @param normalized Pot position in [0, 1]; 0.5 is noon.
        void set_mid(float normalized);

        /// @brief Sets VR5, the HIGH control.
        /// @param normalized Pot position in [0, 1]; 0.5 is noon.
        void set_high(float normalized);

        /// @brief Sets VR7, the lead channel's POST GAIN. It sits ahead of the driver, the phase
        /// inverter and the output tubes, so it drives the power section rather than trimming the
        /// finished signal.
        /// @param decibels Post gain in [-60, +12] dB; 0 dB is the pot wide open.
        void set_post_gain(float decibels);

        /// @brief Sets VR8, RESONANCE.
        /// @param normalized Pot position in [0, 1]; 0 is flat and 1 is the full shelf.
        void set_resonance(float normalized);

        /// @brief Sets VR9, PRESENCE.
        /// @param normalized Pot position in [0, 1]; 0.5 is flat.
        void set_presence(float normalized);

        /// @brief Sets the input trim applied ahead of the grid stopper.
        /// @param decibels Trim in [-24, +24] dB; 0 dB passes the host's level through unchanged.
        void set_input_trim(float decibels);

        /// @brief Sets the output level applied after the cabinet stage.
        /// @param decibels Output level in [-60, +12] dB; 0 dB is unity.
        void set_output(float decibels);

        /// @brief Linear gain the finished signal is scaled by so an untouched amp turns a
        /// -12 dBFS 82 Hz input into a -6.00 dBFS peak, the calibration point all four
        /// amplifiers share (docs/superpowers/specs/2026-09-07-grid-knee-antialiasing-design.md
        /// section 6). Re-measured 2026-09-07 on the Norman grid law, the solved inverter bias and
        /// the audio-taper PRE GAIN; 0.6114945 before, pinned at 1 kHz. Measured by
        /// Peavey5150DefaultSettingsPeakNearMinusSixDbfsForAMinusTwelveDbfsInput, which pins it.
        ///
        /// It sits outside the nonlinearity, after the decimator and both feedback shelves, where
        /// Jcm800.cpp applies its kMasterUnityGain inside the amplifier alongside the master pot.
        /// A deliberate departure from the sibling: this amplifier's POST GAIN pot is a real
        /// mid-chain control that drives the driver, the phase inverter and the output tubes, so it
        /// is applied in the chain where the schematic puts it, and this constant is left outside
        /// as a pure level trim. Folding the two together would make a calibration change a tone
        /// change. See modules/peavey5150/README.md.
        static constexpr float kPostGainUnityGain = 0.2400876f;

        /// @brief Overrides the 6L6GC quad's grid bias mismatch. A test hook, not a user control
        /// and not in the descriptor. Audio-thread safe.
        /// @param volts Input-referred grid offset applied to the second half, in volts.
        void set_power_tube_grid_bias_mismatch_volts(float volts);

        /// @brief How many TriodeStages this amplifier owns. A test hook, not a user control and
        /// not in the descriptor.
        /// @return 8: V1A, V1B, V2A, V2B, V5B, V5A, V3A and the phase inverter, in signal order.
        std::size_t triode_stage_count() const;

        /// @brief Reads one triode's largest grid excursion since the last reset(). A test hook,
        /// not a user control and not in the descriptor. Audio-thread safe.
        /// @param stage Index into the signal order triode_stage_count() describes.
        /// @return The peak |Vg - Vk| in volts, or 0 when stage is out of range.
        float grid_excursion_peak_volts(std::size_t stage) const;

        /// @brief Whether a tone, resonance or presence target is still waiting for the audio
        /// thread to rebuild its coefficients.
        /// @return True while any of those coefficient sets is out of date.
        bool tone_stack_is_dirty() const;

        float get_parameter(const char* id) const override;
        void set_parameter(const char* id, float value) override;

        using IAudioNode::set_parameter;

        /// @brief Sets one parameter by its descriptor index. Audio-thread safe.
        /// @param index Index into peavey5150_descriptor()'s parameter array.
        /// @param value The new value, in the descriptor's units.
        void set_parameter(std::uint16_t index, float value) override;

    private:
        static constexpr std::uint32_t kOversamplingFactor = 8;

        double sample_rate_ = 48000.0;
        std::uint32_t cabinet_partition_ = 0;
        std::vector<float> cabinet_impulse_response_;

        SmoothedParameter pre_gain_{ 0.5f }, resonance_{ 0.5f }, presence_{ 0.5f };
        SmoothedParameter low_{ 0.5f }, mid_{ 0.5f }, high_{ 0.5f };
        SmoothedParameter post_gain_{ 1.0f }, input_trim_gain_{ 1.0f }, output_gain_{ 1.0f };
        float post_gain_db_ = 0.0f, input_trim_db_ = 0.0f, output_db_ = 0.0f;
        float power_tube_grid_bias_mismatch_volts_ = 0.0f;
        float applied_low_ = -1.0f, applied_mid_ = -1.0f, applied_high_ = -1.0f;
        float applied_presence_ = -1.0f, applied_resonance_ = -1.0f;

        Biquad input_low_pass_;    ///< R22 68k against C20 39p.
        Biquad c6_coupling_;       ///< V1A's plate into the lead pre-gain network.
        Biquad pre_gain_peaker_;   ///< R29 470k in parallel with C17 2.2n.
        Biquad c2_coupling_;       ///< V1B's plate into the R6/R9 divider.
        Biquad c1_coupling_;       ///< V2A's plate into its output node.
        Biquad c48_low_pass_;      ///< C48 1n against that node's shunt.
        Biquad c57_coupling_;      ///< V2B's plate through R32 into R101.
        Biquad c56_coupling_;      ///< V5B's plate through R87 and R88 into R91.
        Biquad c52_coupling_;      ///< The post-gain node into V3B's grid.
        Biquad c11_coupling_;      ///< The post-gain node into V3A's grid.
        Biquad c49_coupling_;      ///< V3A's plate through R48 into the pair's grid.

        Biquad v1a_cathode_shelf_; ///< R12 1.02k partially bypassed by C3 1u.
        Biquad v1b_cathode_shelf_; ///< R17 1.02k partially bypassed by C4 1u.
        Biquad v2b_cathode_shelf_; ///< R13 1.02k partially bypassed by C5 1u.
        Biquad v3a_cathode_shelf_; ///< R14 3.3k with C61 4.7n, an oscillation stopper.

        // One-pole grid-input-capacitance rolloffs, one per driven grid; the Miller constants in
        // Peavey5150.cpp set their corners.
        Biquad v1b_grid_miller_;
        Biquad v2a_grid_miller_;
        Biquad v2b_grid_miller_;
        Biquad v5b_grid_miller_;
        Biquad v5a_grid_miller_;
        Biquad v3a_grid_miller_;
        Biquad phase_inverter_grid_miller_;

        Biquad presence_shelf_;

        TriodeStage v1a_;
        TriodeStage v1b_;
        TriodeStage v2a_;
        TriodeStage v2b_;
        TriodeStage v5b_;
        TriodeStage v5a_;
        Peavey5150ToneStack tone_stack_;
        TriodeStage v3a_;
        TriodeStage phase_inverter_;
        PentodeStage power_tube_a_;
        PentodeStage power_tube_b_;
        OutputTransformerStage output_transformer_;
        PowerSupplySagStage sag_;
        ResonanceControl resonance_shelf_;

        std::unique_ptr<Oversampler8x> oversampler_;  // spans V1A -> sag
        RtSwapSlot<StreamingConvolution> cabinet_slot_;

        std::vector<float> scratch_base_;
        std::vector<float> scratch_oversampled_;
    };

    /// @brief The 5150's product registration entry point.
    /// @return The static NodeDescriptor for "sushidsp.peavey5150".
    const NodeDescriptor& peavey5150_descriptor();
}
