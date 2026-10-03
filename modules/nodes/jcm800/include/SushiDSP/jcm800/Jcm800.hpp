/**************************************************************************/
/* Jcm800.hpp                                                             */
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
#include "SushiDSP/core/NodeDescriptor.hpp"
#include "SushiDSP/core/ParameterTable.hpp"
#include "SushiDSP/jcm800/JcmToneStack.hpp"
#include "SushiDSP/circuits/stages/OutputTransformerStage.hpp"
#include "SushiDSP/circuits/stages/PentodeStage.hpp"
#include "SushiDSP/circuits/stages/PowerSupplySagStage.hpp"
#include "SushiDSP/circuits/stages/TriodeStage.hpp"
#include "SushiDSP/math/Biquad.hpp"
#include "SushiDSP/math/Oversampler.hpp"
#include "SushiDSP/math/SmoothedParameter.hpp"

namespace SushiDSP
{
    /// @brief The Marshall JCM800 2203 amp: bright cap, V1a/V1b/tone-stack/V2a/phase-inverter/
    /// EL34-push-pull, output transformer, power sag, presence, followed by a plain full-scale
    /// clamp. Cabinet convolution lives in the standalone sushidsp.cabinet node. Exposes
    /// jcm800_descriptor().
    class Jcm800 : public IAudioNode
    {
    public:
        Jcm800();

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

        /// @brief Reports the delay this node adds from input to output, so the host can
        /// compensate it.
        /// @return The single oversampled region's latency.
        std::uint32_t latency_samples() const override;

        /// @brief This amp's registered type id.
        /// @return "sushidsp.jcm800".
        const char* type_id() const override;

        /// @brief Sets the preamp (gain) control.
        /// @param normalized Preamp amount in [0, 1]; smoothed toward on the audio thread.
        void set_preamp(float normalized);

        /// @brief Linear gain the power section is scaled by when the master knob reads 0 dB.
        /// Pinned by Jcm800DefaultSettingsPeakNearMinusSixDbfsForAMinusTwelveDbfsInput: an
        /// untouched amp turns a -12 dBFS 82 Hz sine into a -6.00 dBFS peak, the calibration
        /// point all four amplifiers share (docs/archive/superpowers/specs/
        /// 2026-09-07-grid-knee-antialiasing-design.md section 6). Re-measured 2026-09-07 on the
        /// Norman grid law, on the volt-true power stage and on the transformer whose secondary
        /// carries no mean; 0.2534701, 0.3105599 and 0.3101991 before those. It multiplies the
        /// signal after the last nonlinearity, so it scales the output and nothing else.
        static constexpr float kMasterUnityGain = 0.2500494f;

        /// @brief Sets the master volume.
        /// @param decibels Master level in [-60, +12] dB; 0 dB is the amp's reference loudness.
        void set_master(float decibels);

        /// @brief Sets the input trim applied ahead of the bright cap.
        /// @param decibels Trim in [-24, +24] dB; 0 dB passes the host's level through unchanged.
        void set_input_trim(float decibels);

        /// @brief Sets the output level applied after the power section, this node's final trim before
        /// handing the dry signal to the standalone cabinet node.
        /// @param decibels Output level in [-60, +12] dB; 0 dB is unity.
        void set_output(float decibels);

        /// @brief Sets the tone stack's bass control.
        /// @param normalized Bass amount in [0, 1]; 0.5 is flat.
        void set_bass(float normalized);

        /// @brief Sets the tone stack's middle control.
        /// @param normalized Middle amount in [0, 1]; 0.5 is flat.
        void set_middle(float normalized);

        /// @brief Sets the tone stack's treble control.
        /// @param normalized Treble amount in [0, 1]; 0.5 is flat.
        void set_treble(float normalized);

        /// @brief Sets the presence (high-shelf) control.
        /// @param normalized Presence amount in [0, 1]; 0.5 is flat.
        void set_presence(float normalized);

        /// @brief Overrides the EL34 pair's grid bias mismatch. A test/advanced hook, not a
        /// user control and not in the descriptor: it exists so a test can zero the mismatch and
        /// measure what it contributes. The default is the shipped constant in Jcm800.cpp, so
        /// leaving it alone leaves the amp's output unchanged. Audio-thread safe.
        /// @param volts Input-referred grid offset applied to the second EL34, in volts.
        void set_power_tube_grid_bias_mismatch_volts(float volts);

        /// @brief How many triode stages this amp exposes to grid-excursion probing.
        /// @return 4: V1a, V1b, V2a and the phase inverter, in signal order.
        std::size_t triode_stage_count() const;

        /// @brief Reads one triode stage's largest grid excursion since the last reset(). A test
        /// hook like set_power_tube_grid_bias_mismatch_volts(): it exists so a test can read the
        /// excursions behind the spec's section 4 criterion, a clean end at minimum gain, and it
        /// is not in the descriptor.
        /// @param stage Index in signal order, as triode_stage_count() counts them.
        /// @return The peak |Vg - Vk| in volts, or 0 for an out-of-range index.
        float grid_excursion_peak_volts(std::size_t stage) const;

        /// @brief Whether a tone or presence target is still waiting for the audio thread to
        /// rebuild its coefficients. A diagnostic hook: it is what makes "the control thread only
        /// stores a target" observable from a test.
        /// @return True while any tone-stack or presence coefficient is out of date.
        bool tone_stack_is_dirty() const;

        float get_parameter(const char* id) const override;
        void set_parameter_for_control_thread(const char* id, float value) override;


        /// @brief Sets one parameter by its descriptor index. Audio-thread safe.
        /// @param index Index into jcm800_descriptor()'s parameter array.
        /// @param value The new value, in the descriptor's units.
        void set_parameter_for_audio_thread(std::uint16_t index, float value) override;

        /// @brief This node's parameters, one row each, feeding the descriptor and both
        /// dispatches. Declared on the class rather than at namespace scope so a row's
        /// lambdas can reach the private field a knob actually lives in.
        /// @return The table, built once on first use.
        static const ParameterTable<Jcm800, 8>& parameters();

    private:
        static constexpr std::uint32_t kOversamplingFactor = 8;

        double sample_rate_ = 48000.0;

        SmoothedParameter preamp_{ 0.5f }, presence_{ 0.5f };
        SmoothedParameter bass_{ 0.5f }, middle_{ 0.5f }, treble_{ 0.5f };
        SmoothedParameter master_gain_{ 1.0f }, input_trim_gain_{ 1.0f }, output_gain_{ 1.0f };
        float master_db_ = 0.0f, input_trim_db_ = 0.0f, output_db_ = 0.0f;
        float power_tube_grid_bias_mismatch_volts_ = 0.0f;
        float applied_bass_ = -1.0f, applied_middle_ = -1.0f, applied_treble_ = -1.0f, applied_presence_ = -1.0f;

        Biquad bright_cap_;
        Biquad coupling_cap_;
        Biquad presence_shelf_;

        // The 2203's 470 k / 470 pF treble peaker into V1b's grid leak, as the high shelf that
        // network is; see kV1bTreblePeakerDivider in Jcm800.cpp for the values it comes from.
        Biquad v1b_treble_peaker_;

        // One-pole grid-input-capacitance rolloffs, one per driven triode grid; see the Miller
        // constants in Jcm800.cpp for what sets their corners.
        Biquad v1b_grid_miller_;
        Biquad v2a_grid_miller_;
        Biquad phase_inverter_grid_miller_;

        TriodeStage v1a_;
        TriodeStage v1b_;
        JcmToneStack tone_stack_;
        TriodeStage v2a_;
        TriodeStage phase_inverter_;
        PentodeStage el34_a_;
        PentodeStage el34_b_;
        OutputTransformerStage output_transformer_;
        PowerSupplySagStage sag_;

        std::unique_ptr<Oversampler8x> oversampler_;  // spans V1a -> sag

        std::vector<float> scratch_base_;
        std::vector<float> scratch_oversampled_;
    };

    /// @brief The JCM800's product registration entry point.
    /// @return The static NodeDescriptor for "sushidsp.jcm800".
    const NodeDescriptor& jcm800_descriptor();
}
