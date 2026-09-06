/**************************************************************************/
/* MesaMark2c.hpp                                                         */
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
 * @file MesaMark2c.hpp
 * @brief The Mesa/Boogie Mark IIC+ lead-mode amplifier node and its descriptor.
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
#include "SushiDSP/core/RtSwapSlot.hpp"
#include "SushiDSP/io/Ir.hpp"
#include "SushiDSP/mesa_mark2c/MesaMark2cToneStack.hpp"
#include "SushiDSP/circuits/GraphicEqualizer.hpp"
#include "SushiDSP/circuits/stages/OutputTransformerStage.hpp"
#include "SushiDSP/circuits/stages/PentodeStage.hpp"
#include "SushiDSP/circuits/stages/PowerSupplySagStage.hpp"
#include "SushiDSP/circuits/stages/TriodeStage.hpp"
#include "SushiDSP/math/Biquad.hpp"
#include "SushiDSP/math/Oversampler.hpp"
#include "SushiDSP/math/SmoothedParameter.hpp"
#include "SushiDSP/math/StreamingConvolution.hpp"

namespace SushiDSP
{
    /// @brief The Mesa/Boogie Mark IIC+ in Lead mode: V1A, the FMV stack on its plate, Volume 1,
    /// V1B, Lead Drive, V3B, V4A, the lead output divider, Lead Master, V2A, V2B, Master 1, the
    /// five-band graphic equaliser, the long-tailed pair, the Simul-Class output pairs, output
    /// transformer, power sag, presence and an optional cabinet convolution, followed by a plain
    /// full-scale clamp. Exposes mesa_mark2c_descriptor().
    class MesaMark2c : public IAudioNode
    {
    public:
        /// @brief The number of graphic-equaliser sliders on the front panel.
        static constexpr std::size_t kEqualizerBandCount = 5;

        /// @param cabinet_impulse_response Cabinet IR samples; empty means no cabinet stage.
        explicit MesaMark2c(std::vector<float> cabinet_impulse_response = {});

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
        /// reaches the cabinet through the slot's publisher-side accessor.
        void reset() override;

        /// @brief Reports the delay this node adds from input to output.
        /// @return The single oversampled region's latency plus, when a cabinet is loaded, its
        /// one-partition convolution latency. A host should re-read it after set_cabinet().
        std::uint32_t latency_samples() const override;

        /// @brief This amp's registered type id.
        /// @return "sushidsp.mesa_mark2c".
        const char* type_id() const override;

        /// @brief Installs a new cabinet impulse response. Control thread only: the convolver is
        /// built here in full and handed to the audio thread as one atomic pointer swap.
        /// @param impulse_response The IR and the rate it was captured at; its sample_rate must
        /// equal the rate this node was prepared at. The samples are retained so a later
        /// prepare() rebuilds this cabinet; they are never resampled.
        /// @return True if the cabinet was installed; false if the IR is empty, if the rates
        /// disagree, or if prepare() has not run yet, in which case nothing is installed and the
        /// previously active cabinet stays in place.
        bool set_cabinet(Ir impulse_response);

        /// @brief Frees every cabinet the audio thread has provably moved past. Control thread only.
        void reclaim_cabinets();

        /// @brief Whether a cabinet convolution is currently in the signal path.
        /// @return True once a cabinet has been installed by the constructor or set_cabinet().
        bool has_cabinet() const;

        /// @brief Reports how many triode stages the grid-excursion probe can be asked about.
        /// A test hook, like set_bias_mismatch() on the JCM800.
        /// @return 7: V1A, V1B, V3B, V4A, V2A, V2B and the phase inverter, in signal order.
        std::size_t triode_stage_count() const;

        /// @brief Reads one triode's largest grid excursion since the last reset(). A test hook.
        /// @param stage Index in signal order, in [0, triode_stage_count()).
        /// @return The peak |Vg - Vk| that stage has reached, in volts; 0 when stage is out of range.
        float grid_excursion_peak_volts(std::size_t stage) const;

        /// @brief Linear gain the power section is scaled by when Master 1 reads 0 dB. Measured
        /// so an untouched amp turns a -12 dBFS 1 kHz sine into roughly -6 dBFS out, the way the
        /// JCM800's kMasterUnityGain was measured, and pinned by
        /// MesaMark2cDefaultSettingsPeakNearMinusSixDbfsForAMinusTwelveDbfsInput. Re-measured
        /// 2026-09-07 after the long-tailed pair took the cathode shunt and its full stage gain:
        /// the default peak read -5.8379 dBFS, so 0.7040127 x 0.9815106 gives this figure.
        static constexpr float kMasterUnityGain = 0.6909959f;

        /// @brief Sets the Volume 1 control, the 1 M pot between the tone stack and V1B's grid.
        /// @param normalized Volume 1 amount in [0, 1]; smoothed toward on the audio thread.
        void set_volume_1(float normalized);

        /// @brief Sets the tone stack's treble control.
        /// @param normalized Treble pot position in [0, 1].
        void set_treble(float normalized);

        /// @brief Sets the tone stack's bass control.
        /// @param normalized Bass pot position in [0, 1].
        void set_bass(float normalized);

        /// @brief Sets the tone stack's middle control.
        /// @param normalized Middle pot position in [0, 1].
        void set_middle(float normalized);

        /// @brief Sets the Lead Drive control, the 1 M pot ahead of V3B's grid.
        /// @param normalized Lead Drive amount in [0, 1].
        void set_lead_drive(float normalized);

        /// @brief Sets the Lead Master control, the 250 k pot ahead of V2A's grid.
        /// @param normalized Lead Master amount in [0, 1].
        void set_lead_master(float normalized);

        /// @brief Sets Master 1.
        /// @param decibels Master level in [-60, +12] dB; 0 dB is the amp's reference loudness.
        void set_master_1(float decibels);

        /// @brief Sets the presence control in the feedback leg from the 8 ohm tap.
        /// @param normalized Presence amount in [0, 1]; 0.5 is flat.
        void set_presence(float normalized);

        /// @brief Sets one graphic-equaliser slider.
        /// @param band Slider index in [0, kEqualizerBandCount); out-of-range indices are ignored.
        /// @param position Slider position in [-1, 1]; 0 is flat.
        void set_equalizer_band(std::size_t band, float position);

        /// @brief Sets the input trim applied ahead of V1A.
        /// @param decibels Trim in [-24, +24] dB; 0 dB passes the host's level through unchanged.
        void set_input_trim(float decibels);

        /// @brief Sets the output level applied after the cabinet stage.
        /// @param decibels Output level in [-60, +12] dB; 0 dB is unity.
        void set_output(float decibels);

        /// @brief Whether a tone, presence, Lead Drive or equaliser target is still waiting for
        /// the audio thread to rebuild its coefficients.
        /// @return True while any of those coefficient sets is out of date.
        bool tone_stack_is_dirty() const;

        float get_parameter(const char* id) const override;
        void set_parameter(const char* id, float value) override;

        using IAudioNode::set_parameter;

        /// @brief Sets one parameter by its descriptor index. Audio-thread safe.
        /// @param index Index into mesa_mark2c_descriptor()'s parameter array.
        /// @param value The new value, in the descriptor's units.
        void set_parameter(std::uint16_t index, float value) override;

    private:
        static constexpr std::uint32_t kOversamplingFactor = 8;

        /// @brief Rebuilds every filter whose coefficients depend on the sample rate.
        /// @param oversampled_rate_hz The rate the oversampled region runs at, in hertz.
        void build_rate_dependent_filters(double oversampled_rate_hz);

        double sample_rate_ = 48000.0;
        std::uint32_t cabinet_partition_ = 0;
        std::vector<float> cabinet_impulse_response_;

        SmoothedParameter volume_1_{ 0.5f }, treble_{ 0.5f }, bass_{ 0.5f }, middle_{ 0.5f };
        SmoothedParameter lead_drive_{ 0.5f }, lead_master_{ 0.5f }, presence_{ 0.5f };
        SmoothedParameter equalizer_[kEqualizerBandCount]{
            SmoothedParameter{ 0.0f }, SmoothedParameter{ 0.0f }, SmoothedParameter{ 0.0f },
            SmoothedParameter{ 0.0f }, SmoothedParameter{ 0.0f }
        };
        SmoothedParameter master_gain_{ 1.0f }, input_trim_gain_{ 1.0f }, output_gain_{ 1.0f };
        float master_1_db_ = 0.0f, input_trim_db_ = 0.0f, output_db_ = 0.0f;
        float applied_bass_ = -1.0f, applied_middle_ = -1.0f, applied_treble_ = -1.0f;
        float applied_presence_ = -1.0f, applied_lead_drive_ = -1.0f;
        float applied_equalizer_[kEqualizerBandCount]{ -2.0f, -2.0f, -2.0f, -2.0f, -2.0f };

        Biquad v1b_coupling_;
        Biquad lead_drive_bright_;
        Biquad v3b_coupling_;
        Biquad v4a_interstage_shelf_;
        Biquad v4a_cathode_shelf_;
        Biquad v4a_coupling_;
        Biquad lead_output_divider_;
        Biquad v2b_loop_return_coupling_;
        Biquad v2b_cathode_shelf_;
        Biquad v2b_coupling_;
        Biquad presence_shelf_;

        // One-pole grid-input-capacitance rolloffs, one per driven triode grid; the Miller
        // constants in MesaMark2c.cpp set their corners.
        Biquad v1b_grid_miller_;
        Biquad v3b_grid_miller_;
        Biquad v4a_grid_miller_;
        Biquad v2a_grid_miller_;
        Biquad v2b_grid_miller_;
        Biquad phase_inverter_grid_miller_;

        TriodeStage v1a_;
        MesaMark2cToneStack tone_stack_;
        TriodeStage v1b_;
        TriodeStage v3b_;
        TriodeStage v4a_;
        TriodeStage v2a_;
        TriodeStage v2b_;
        GraphicEqualizer equalizer_bank_;
        TriodeStage phase_inverter_;
        PentodeStage power_inner_a_;
        PentodeStage power_inner_b_;
        PentodeStage power_outer_a_;
        PentodeStage power_outer_b_;
        OutputTransformerStage output_transformer_;
        PowerSupplySagStage sag_;

        std::unique_ptr<Oversampler8x> oversampler_;  // spans V1A -> sag
        RtSwapSlot<StreamingConvolution> cabinet_slot_;

        std::vector<float> scratch_base_;
        std::vector<float> scratch_oversampled_;
    };

    /// @brief The Mark IIC+'s product registration entry point.
    /// @return The static NodeDescriptor for "sushidsp.mesa_mark2c".
    const NodeDescriptor& mesa_mark2c_descriptor();
}
