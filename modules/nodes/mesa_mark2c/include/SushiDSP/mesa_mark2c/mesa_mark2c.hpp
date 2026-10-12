/**************************************************************************/
/* mesa_mark2c.hpp                                                        */
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
 * @file mesa_mark2c.hpp
 * @brief The Mesa/Boogie Mark IIC+ lead-mode amplifier node and its descriptor.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "SushiDSP/core/calibration.hpp"
#include "SushiDSP/core/audio_node.hpp"
#include "SushiDSP/core/node_descriptor.hpp"
#include "SushiDSP/core/parameter_table.hpp"
#include "SushiDSP/mesa_mark2c/mesa_mark2c_tone_stack.hpp"
#include "SushiDSP/circuits/graphic_equalizer.hpp"
#include "SushiDSP/circuits/power_amp_feedback.hpp"
#include "SushiDSP/circuits/stages/long_tailed_pair.hpp"
#include "SushiDSP/circuits/stages/output_transformer_stage.hpp"
#include "SushiDSP/circuits/stages/pentode_stage.hpp"
#include "SushiDSP/circuits/stages/power_supply_sag_stage.hpp"
#include "SushiDSP/circuits/stages/triode_stage.hpp"
#include "SushiDSP/math/biquad.hpp"
#include "SushiDSP/math/oversampler.hpp"
#include "SushiDSP/math/smoothed_parameter.hpp"

namespace SushiDSP
{
    /// @brief The Mesa/Boogie Mark IIC+ in Lead mode: V1A, the FMV stack on its plate, Volume 1,
    /// V1B, Lead Drive, V3B, V4A, the lead output divider, V2A, the effects loop network with its
    /// Lead Master, V2B, Master 1, the five-band graphic equaliser, the long-tailed pair, the
    /// Simul-Class output pairs, output transformer, power sag and presence, followed by a plain
    /// full-scale clamp. Cabinet convolution lives in the standalone sushidsp.cabinet node.
    /// Exposes mesa_mark2c_descriptor().
    class MesaMark2c : public AudioNode
    {
        public:
            /// @brief The number of graphic-equaliser sliders on the front panel.
            static constexpr std::size_t K_EQUALIZER_BAND_COUNT = 5;

            MesaMark2c();

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
            /// @return "sushidsp.mesa_mark2c".
            const char* type_id() const override;

            /// @brief Reports how many triode stages the grid-excursion probe can be asked about.
            /// A test hook, like set_bias_mismatch() on the JCM800.
            /// @return 7: V1A, V1B, V3B, V4A, V2A, V2B and the phase inverter, in signal order.
            std::size_t triode_stage_count() const;

            /// @brief Reads one triode's largest grid excursion since the last reset(). A test hook.
            /// @param stage Index in signal order, in [0, triode_stage_count()).
            /// @return The peak |Vg - Vk| that stage has reached, in volts; 0 when stage is out of range.
            float grid_excursion_peak_volts(std::size_t stage) const;

            /// @brief Linear gain node F is scaled by when Master 1 reads 0 dB, the pot's fraction
            /// ahead of the equaliser and the phase inverter. Measured so an untouched amp turns a
            /// -12 dBFS 82 Hz sine into a -6.00 dBFS peak, the calibration point all four amplifiers
            /// share (docs/archive/superpowers/specs/2026-09-07-grid-knee-antialiasing-design.md section 6),
            /// and pinned by MesaMark2cDefaultSettingsPeakNearMinusSixDbfsForAMinusTwelveDbfsInput.
            /// Re-measured 2026-09-08 with Master 1 at node F and the loop network in place
            /// (docs/archive/superpowers/specs/2026-09-08-iicplus-loop-network-design.md); 0.2349335,
            /// 0.2402319, 0.3343019 and 0.6907759 before, when it scaled the output, the last at 1 kHz.
            static constexpr float K_MASTER_UNITY_GAIN = 0.0038289f;

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

            /// @brief Sets the Lead Master control, the 250 k rheostat on V2A's coupled plate.
            /// @param normalized Lead Master amount in [0, 1].
            void set_lead_master(float normalized);

            /// @brief Sets Master 1.
            /// @param decibels Master level in [-60, +12] dB; 0 dB is the amp's reference loudness.
            void set_master_1(float decibels);

            /// @brief Sets the presence control in the feedback leg from the 8 ohm tap.
            /// @param normalized Presence amount in [0, 1]; 0.5 is flat.
            void set_presence(float normalized);

            /// @brief Sets one graphic-equaliser slider.
            /// @param band Slider index in [0, K_EQUALIZER_BAND_COUNT); out-of-range indices are ignored.
            /// @param position Slider position in [-1, 1]; 0 is flat.
            void set_equalizer_band(std::size_t band, float position);

            /// @brief The three positions of the panel's equaliser switch (owner's manual page 19).
            enum class EqualizerMode
            {
                Out, ///< Centre: LDR5 lifts the slider bus off ground and the traps leave the signal.
                In,  ///< Down: the equaliser is engaged whatever the channel is doing.
                Auto ///< Up: engaged with the lead channel, which is the only channel this node has.
            };

            /// @brief Sets the equaliser switch.
            /// @param mode Which of the three panel positions the switch is in; Auto behaves as In
            /// here, because this node is the lead chain and nothing else selects a channel.
            void set_equalizer_mode(EqualizerMode mode);

            /// @brief Sets PULL BRIGHT, which is two capacitors on one switch.
            /// @param engaged True puts a second 250 pF beside the tone stack's treble capacitor and
            /// 180 pF across Volume 1's top section.
            void set_bright(bool engaged);

            /// @brief Sets PULL DEEP, the second bypass capacitor on V2B's cathode.
            /// @param engaged True adds 15 uF beside the 0.47 uF, taking the cathode's corner from
            /// 339 Hz to 10.3 Hz. Deep does not add bass; it stops the stage removing it.
            void set_deep(bool engaged);

            /// @brief Sets the input trim applied ahead of V1A.
            /// @param decibels Trim in [-24, +24] dB; 0 dB passes the host's level through unchanged.
            void set_input_trim(float decibels);

            /// @brief Sets the output level applied after the power section.
            /// @param decibels Output level in [-60, +12] dB; 0 dB is unity.
            void set_output(float decibels);

            /// @brief Whether a tone, presence, Lead Drive or equaliser target is still waiting for
            /// the audio thread to rebuild its coefficients.
            /// @return True while any of those coefficient sets is out of date.
            bool tone_stack_is_dirty() const;

            float get_parameter(const char* id) const override;
            void set_parameter_for_control_thread(const char* id, float value) override;

            /// @brief Sets one parameter by its descriptor index. Audio-thread safe.
            /// @param index Index into mesa_mark2c_descriptor()'s parameter array.
            /// @param value The new value, in the descriptor's units.
            void set_parameter_for_audio_thread(std::uint16_t index, float value) override;

            /// @brief This node's parameters, one row each, feeding the descriptor and both
            /// dispatches. Declared on the class rather than at namespace scope so a row's
            /// lambdas can reach the private field a knob actually lives in.
            /// @return The table, built once on first use.
            static const ParameterTable<MesaMark2c, 18>& parameters();

        private:
            static constexpr std::uint32_t K_OVERSAMPLING_FACTOR = 8;

            /// @brief Rebuilds every filter whose coefficients depend on the sample rate.
            /// @param oversampled_rate_hz The rate the oversampled region runs at, in hertz.
            void build_rate_dependent_filters(double oversampled_rate_hz);

            /// @brief Advances every knob, rebuilds whichever coefficients moved, and renders that far.
            ///
            /// A host block is rendered as a run of these, so a knob under a sweep reaches the audio
            /// in steps of one control period rather than one block.
            /// @param input The block's input, offset to this period's first frame.
            /// @param output Where this period's output goes.
            /// @param frames How many base-rate frames this period covers.
            void render_control_period(const float* input, float* output, std::uint32_t frames);

            double sample_rate_ = 48000.0;

            SmoothedParameter volume_1_{0.5f}, treble_{0.5f}, bass_{0.5f}, middle_{0.5f};
            SmoothedParameter lead_drive_{0.5f}, lead_master_{0.5f}, presence_{0.5f};
            SmoothedParameter equalizer_[K_EQUALIZER_BAND_COUNT]{
                SmoothedParameter{0.0f}, SmoothedParameter{0.0f}, SmoothedParameter{0.0f},
                SmoothedParameter{0.0f}, SmoothedParameter{0.0f}};
            SmoothedParameter master_gain_{1.0f}, input_trim_gain_{1.0f}, output_gain_{1.0f};
            float master_1_db_ = 0.0f, input_trim_db_ = 0.0f, output_db_ = 0.0f;
            float applied_bass_ = -1.0f, applied_middle_ = -1.0f, applied_treble_ = -1.0f;
            float applied_presence_ = -1.0f, applied_lead_drive_ = -1.0f, applied_volume_1_ = -1.0f;
            float applied_equalizer_[K_EQUALIZER_BAND_COUNT]{-2.0f, -2.0f, -2.0f, -2.0f, -2.0f};

            Biquad v1b_coupling_;
            Biquad lead_drive_bright_;

            /// @brief Volume 1's bright capacitor, flat until PULL BRIGHT switches it across the pot.
            Biquad volume_1_bright_;
            Biquad v3b_coupling_;
            Biquad v4a_interstage_shelf_;
            Biquad v4a_cathode_shelf_;
            Biquad v4a_coupling_;
            Biquad lead_output_divider_;
            Biquad v2b_loop_return_coupling_;
            Biquad v2b_cathode_shelf_;
            Biquad v2b_coupling_;

            // One-pole rolloffs, one per driven triode grid; mesa_mark2c.cpp sets their corners.
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

            /// @brief Where the panel's equaliser switch stands; Out takes the traps out of the path.
            EqualizerMode equalizer_mode_ = EqualizerMode::In;

            /// @brief Whether PULL BRIGHT is in, which adds a treble capacitor at two places at once.
            bool bright_ = false;

            /// @brief Whether PULL DEEP is out, which is what the panel's knob does when pushed in.
            bool deep_ = false;

            /// @brief The feedback leg from the transformer's tap to the inverter's second grid.
            PowerAmpFeedback feedback_;

            /// @brief The phase inverter, the forward signal on one grid and the leg on the other.
            LongTailedPair phase_inverter_;

            /// @brief The previous oversampled sample at the transformer's secondary, which is what
            /// the leg feeds back; the loop's one-sample delay lives here.
            float tap_feedback_ = 0.0f;
            PentodeStage power_inner_a_;
            PentodeStage power_inner_b_;
            PentodeStage power_outer_a_;
            PentodeStage power_outer_b_;
            OutputTransformerStage output_transformer_;
            PowerSupplySagStage sag_;

            std::unique_ptr<Oversampler8x> oversampler_; // spans V1A -> sag

            std::vector<float> scratch_base_;
            std::vector<float> scratch_oversampled_;
    };

    /// @brief The Mark IIC+'s product registration entry point.
    /// @return The static NodeDescriptor for "sushidsp.mesa_mark2c".
    const NodeDescriptor& mesa_mark2c_descriptor();
} // namespace SushiDSP
