/**************************************************************************/
/* DiezelVh4.hpp                                                          */
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
 * @file DiezelVh4.hpp
 * @brief The Diezel VH4 channel-3 amplifier node and its descriptor.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "SushiDSP/core/Calibration.hpp"
#include "SushiDSP/core/IAudioNode.hpp"
#include "SushiDSP/core/NodeDescriptor.hpp"
#include "SushiDSP/diezel_vh4/DiezelVh4ToneStack.hpp"
#include "SushiDSP/circuits/stages/OutputTransformerStage.hpp"
#include "SushiDSP/circuits/stages/PentodeStage.hpp"
#include "SushiDSP/circuits/stages/PowerSupplySagStage.hpp"
#include "SushiDSP/circuits/stages/ResonanceControl.hpp"
#include "SushiDSP/circuits/stages/TriodeStage.hpp"
#include "SushiDSP/math/Biquad.hpp"
#include "SushiDSP/math/Oversampler.hpp"
#include "SushiDSP/math/SmoothedParameter.hpp"

namespace SushiDSP
{
    /// @brief The Diezel VH4's channel 3 ("Mega"): V1a, the channel gain pot's 1 nF split, V2a,
    /// V3a, the V3b cathode follower, the FMV deck, the channel volume, the four-way mix bus,
    /// V4a and the V4b send buffer, then a phase inverter, an EL34 quad, the output transformer,
    /// power sag, Deep and Presence, followed by a full-scale clamp. Cabinet convolution lives
    /// in the standalone sushidsp.cabinet node.
    ///
    /// A documented approximation rather than a white-box model: the gain chain comes from one
    /// reverse-engineering drawing, the tone stack's interconnection is a reconstruction and the
    /// power section is built from prose alone. modules/nodes/diezel_vh4/README.md says which is which.
    /// Exposes diezel_vh4_descriptor().
    class DiezelVh4 : public IAudioNode
    {
    public:
        DiezelVh4();

        /// @brief Sizes every scratch buffer and rebuilds every sample-rate-dependent filter.
        /// @param info Sample rate, max block size, and channel count for this node's lifetime.
        void prepare(const PrepareInfo& info) override;

        /// @brief Runs one block through the full amp chain. Allocation-free.
        /// @param context The per-block input/output buffers and frame count.
        void process(ProcessContext& context) override;

        /// @brief Returns this node's fixed input/output channel counts.
        /// @return Mono in, mono out.
        PortLayout ports() const override;

        /// @brief Clears every stage's state for a click-free restart.
        void reset() override;

        /// @brief Reports the delay this node adds from input to output.
        /// @return The single oversampled region's latency.
        std::uint32_t latency_samples() const override;

        /// @brief This amp's registered type id.
        /// @return "sushidsp.diezel_vh4".
        const char* type_id() const override;

        /// @brief How many triode stages this amp exposes to grid-excursion probing. A test hook
        /// like Jcm800::set_power_tube_grid_bias_mismatch_volts(): it exists so a test can prove
        /// no grid sits on the solver's bound.
        /// @return 5 - V1a, V2a, V3a, V4a and the phase inverter, in signal order.
        std::size_t triode_stage_count() const;

        /// @brief Reads one stage's largest grid excursion since the last reset(). A test hook.
        /// @param stage Index into the signal order triode_stage_count() describes.
        /// @return The peak |Vg - Vk| in volts, or 0 when the index is out of range.
        float grid_excursion_peak_volts(std::size_t stage) const;

        /// @brief Linear gain the power section is scaled by when the master knob reads 0 dB.
        /// It multiplies the signal after the last nonlinearity, so it scales the output and
        /// nothing else. The calibration point is the one all four amplifiers share: every knob
        /// at its default and a -12 dBFS 82 Hz input peaks at -6.00 dBFS (docs/superpowers/specs/
        /// 2026-09-07-grid-knee-antialiasing-design.md section 6), pinned by
        /// DiezelVh4DefaultSettingsPeakNearMinusSixDbfsForAMinusTwelveDbfsInput. Re-measured
        /// 2026-09-07 on the Norman grid law, on the volt-true power stage and on the
        /// transformer whose secondary carries no mean; 0.2365060, 0.3543949 and 0.627263 before
        /// those, the last pinned at 1 kHz. modules/nodes/diezel_vh4/README.md carries the derivation.
        static constexpr float kMasterUnityGain = 0.2319680f;

        /// @brief Sets the channel-3 GAIN control, the 250 kOhm pot of dossier section 3.2.
        /// @param normalized Gain amount in [0, 1]; it moves the input high-pass corner as well as
        /// the level, which is what voices this channel.
        void set_gain(float normalized);

        /// @brief Sets the tone stack's bass control.
        /// @param normalized Bass pot position in [0, 1].
        void set_bass(float normalized);

        /// @brief Sets the tone stack's middle control.
        /// @param normalized Middle pot position in [0, 1].
        void set_middle(float normalized);

        /// @brief Sets the tone stack's treble control.
        /// @param normalized Treble pot position in [0, 1].
        void set_treble(float normalized);

        /// @brief Sets the channel volume, the 470 kOhm pot feeding the mix bus.
        /// @param normalized Volume amount in [0, 1].
        void set_volume(float normalized);

        /// @brief Sets the master section's Deep control.
        /// @param normalized Deep amount in [0, 1]; 0 is flat and 1 is the full low-shelf lift.
        void set_deep(float normalized);

        /// @brief Sets the master section's Presence control.
        /// @param normalized Presence amount in [0, 1]; 0.5 is flat.
        void set_presence(float normalized);

        /// @brief Sets the master volume.
        /// @param decibels Master level in [-60, +12] dB; 0 dB is the amp's reference loudness.
        void set_master(float decibels);

        /// @brief Sets the input trim applied ahead of the input network.
        /// @param decibels Trim in [-24, +24] dB; 0 dB passes the host's level through unchanged.
        void set_input_trim(float decibels);

        /// @brief Sets the output level applied after the power section, this node's final trim before
        /// handing the dry signal to the standalone cabinet node.
        /// @param decibels Output level in [-60, +12] dB; 0 dB is unity.
        void set_output(float decibels);

        /// @brief Whether a tone, Deep, Presence or gain-corner target is still waiting for the
        /// audio thread to rebuild its coefficients. A diagnostic hook.
        /// @return True while any of those coefficient sets is out of date.
        bool tone_stack_is_dirty() const;

        float get_parameter(const char* id) const override;
        void set_parameter_for_control_thread(const char* id, float value) override;


        /// @brief Sets one parameter by its descriptor index. Audio-thread safe.
        /// @param index Index into diezel_vh4_descriptor()'s parameter array.
        /// @param value The new value, in the descriptor's units.
        void set_parameter_for_audio_thread(std::uint16_t index, float value) override;

    private:
        static constexpr std::uint32_t kOversamplingFactor = 8;

        /// @brief Rebuilds the gain pot's coupling high-pass for one knob position.
        /// @param normalized The gain knob position the corner is derived from.
        void rebuild_gain_high_pass(float normalized);

        double sample_rate_ = 48000.0;

        SmoothedParameter gain_{ 0.5f }, bass_{ 0.5f }, middle_{ 0.5f }, treble_{ 0.5f };
        SmoothedParameter volume_{ 0.5f }, deep_{ 0.5f }, presence_{ 0.5f };
        SmoothedParameter master_gain_{ 1.0f }, input_trim_gain_{ 1.0f }, output_gain_{ 1.0f };
        float master_db_ = 0.0f, input_trim_db_ = 0.0f, output_db_ = 0.0f;
        float applied_gain_ = -1.0f, applied_bass_ = -1.0f, applied_middle_ = -1.0f;
        float applied_treble_ = -1.0f, applied_deep_ = -1.0f, applied_presence_ = -1.0f;

        Biquad gain_high_pass_;
        Biquad v2a_cathode_shelf_;
        Biquad v2a_grid_miller_;
        Biquad v3a_coupling_high_pass_;
        Biquad v3a_cathode_shelf_;
        Biquad v3a_grid_miller_;
        Biquad follower_high_pass_;
        Biquad presence_shelf_;

        TriodeStage v1a_;
        TriodeStage v2a_;
        TriodeStage v3a_;
        DiezelVh4ToneStack tone_stack_;
        TriodeStage v4a_;
        TriodeStage phase_inverter_;
        PentodeStage el34_a_;
        PentodeStage el34_b_;
        OutputTransformerStage output_transformer_;
        PowerSupplySagStage sag_;
        ResonanceControl deep_shelf_;

        std::unique_ptr<Oversampler8x> oversampler_;  // spans V1a -> sag

        std::vector<float> scratch_base_;
        std::vector<float> scratch_oversampled_;
    };

    /// @brief The VH4 channel 3 product registration entry point.
    /// @return The static NodeDescriptor for "sushidsp.diezel_vh4".
    const NodeDescriptor& diezel_vh4_descriptor();
}
