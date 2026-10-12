/**************************************************************************/
/* jcm800.cpp                                                             */
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
 * @file jcm800.cpp
 * @brief Implements the JCM800 2203 amplifier node from its preamp to its output clamp.
 * @author Mustafa Garip
 */

#include "SushiDSP/jcm800/jcm800.hpp"

#include "SushiDSP/circuits/audio_taper_pot.hpp"
#include "SushiDSP/circuits/output_safety_clamp.hpp"
#include "SushiDSP/circuits/stages/miller_input_capacitance.hpp"
#include "SushiDSP/circuits/stages/phase_inverter_presets.hpp"
#include "SushiDSP/circuits/stages/power_tube_presets.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>

namespace SushiDSP
{
    namespace
    {
        /// @brief EL34 as the JCM800 2203 runs it: the Philips EL34 datasheet's class B push-pull
        /// point at Vg1 -38 V, Raa 3.4 kOhm, 2 x 30 mA, 55 W (modules/nodes/jcm800/README.md, "The EL34
        /// pair"); source (82k || 43.056k) + 5.6k from the corrected inverter and jcm_pp_ot_nfb.
        constexpr PowerTubePreset K_EL34{33831.9f,
                                         220000.0f,
                                         -38.0f,
                                         6.882361f,
                                         K_PENTODE_EL34_GRID_CONDUCTION,
                                         PlateSwing{25.5f, 280.2777f}};

        // Component values are approximations, not measured; see modules/nodes/jcm800/README.md.
        constexpr float K_TRIODE_SOURCE_RESISTANCE = 10000.0f;
        constexpr float K_TRIODE_GRID_LEAK = 1000000.0f;

        /// @brief V1a's gain, mu*Ra/(ra + Ra) from the Koren Jacobian at its idle point below:
        /// gm 1.5025 mA/V, ra 58.465 kOhm, mu 87.8412 on the 100 kOhm plate, the cathode bypassed.
        constexpr float K_V1A_GAIN = 55.432497f;

        /// @brief V1b's gain with its 10 kOhm cathode unbypassed, mu*Ra/(ra + Ra + (mu + 1)*Rk) at
        /// its idle point: gm 0.6812 mA/V, ra 115.768 kOhm, mu 78.8604; jcm_v1b_cold's AC sweep
        /// reads +16.89 dB (docs/archive/superpowers/specs/2026-09-07-grid-knee-antialiasing-design.md
        /// section 6).
        constexpr float K_V1B_GAIN = 7.774305f;

        /// @brief V2a's gain, mu*Ra/(ra + Ra) at its idle point below: gm 2.2682 mA/V, ra
        /// 41.923 kOhm, mu 95.0908 on the 100 kOhm plate, the cathode bypassed.
        constexpr float K_V2A_GAIN = 67.001549f;

        /// @brief V1a's self-bias voltage: Rk 2.7 kOhm carrying the 0.855584 mA the Koren model
        /// settles at against a 100 kOhm plate load on a 360 V supply (jcm_v1a).
        constexpr float K_V1A_CATHODE_BIAS = -2.310076f;

        /// @brief V1a's plate swing: 360 - 274.441625 toward the supply, 274.441625 - 120.747068
        /// toward the plate bottom at Vgk = 0 on the same load line.
        constexpr PlateSwing K_V1A_PLATE_SWING{85.558375f, 153.694557f};

        /// @brief V1b's self-bias voltage: the cold clipper's unbypassed 10 kOhm cathode carrying
        /// 0.340132 mA against a 100 kOhm plate load on 360 V (jcm_v1b_cold).
        constexpr float K_V1B_CATHODE_BIAS = -3.401321f;

        /// @brief V1b's plate swing: 360 - 325.986790 toward the supply, 325.986790 - 121.553151
        /// toward the plate bottom at Vgk = 0, the cathode held at its idle voltage.
        constexpr PlateSwing K_V1B_PLATE_SWING{34.013210f, 204.433638f};

        /// @brief V2a's self-bias voltage: an 820 Ohm bypassed cathode carrying 1.519346 mA
        /// against a 100 kOhm plate load on 360 V (jcm_cf's V2a half).
        constexpr float K_V2A_CATHODE_BIAS = -1.245863f;

        /// @brief V2a's plate swing: 360 - 208.065440 toward the supply, 208.065440 - 119.960758
        /// toward the plate bottom at Vgk = 0 on the same load line.
        constexpr PlateSwing K_V2A_PLATE_SWING{151.934560f, 88.104683f};

        /// @brief The 2203's long-tailed pair, modelled as one triode on jcm_ltp's 82 kOhm plate
        /// over the 470 Ohm and 10 kOhm tail both cathodes share, with both grid leaks returned to
        /// the junction between them as the 2203 wires them; every figure is derived in
        /// modules/foundation/circuits/include/SushiDSP/circuits/stages/phase_inverter_presets.hpp.
        constexpr SingleTriodeInverter K_PHASE_INVERTER = K_TRIODE12_AX7_INVERTER82K_PLATE;

        /// @brief The class-AB bias mismatch between the two output tubes, an input-referred grid
        /// offset on the B side in real volts: the 0.03 model volts the chain carried before
        /// PentodeStage became volt-true, through the deleted 0.125 grid scale
        /// (modules/nodes/jcm800/README.md, "The EL34 pair"). The transformer's 60 Hz high-pass blocks
        /// the DC term it introduces. Held in a member so that
        /// set_power_tube_grid_bias_mismatch_volts() can zero it.
        constexpr float K_POWER_TUBE_GRID_BIAS_MISMATCH_VOLTS = 0.24f;

        // One 100 kOhm plate load drives all three grids; see modules/nodes/jcm800/README.md.
        constexpr float K_PLATE_LOAD_OHMS = 100000.0f;

        constexpr float K_OUTPUT_TRANSFORMER_ROLLOFF_HZ = 60.0f;
        constexpr float K_OUTPUT_TRANSFORMER_LEAKAGE_HZ = 8000.0f;
        constexpr float K_OUTPUT_TRANSFORMER_SATURATION_DRIVE = 0.3f;

        constexpr float K_SAG_AMOUNT = 0.3f;
        constexpr float K_SAG_RECOVERY_TIME_SECONDS = 0.05f;

        /// @brief The ceiling of the preamp pot's attenuation, reached with the knob full up.
        constexpr float K_PREAMP_POT_MAX_ATTENUATION = 1.0f / 16.0f;

        /// @brief The level the output transformer stage receives at the pair's full clip: the
        /// 2 x 20 V x 0.05 the chain delivered before PentodeStage became volt-true.
        constexpr float K_TRANSFORMER_FULL_CLIP_UNITS = 2.0f;

        /// @brief The output trim from the EL34 plates to the transformer's units, applied after
        /// the last grid, so it sets output level rather than any stage's drive: the pair's
        /// full swing, K_EL34's two headrooms summed, lands at K_TRANSFORMER_FULL_CLIP_UNITS.
        constexpr float K_POWER_TUBE_TO_TRANSFORMER_ATTENUATION =
            K_TRANSFORMER_FULL_CLIP_UNITS / (K_EL34.plate_swing.headroom_to_supply_volts +
                                             K_EL34.plate_swing.headroom_to_bottom_volts);

        /// @brief The mid-band loss of the 2203's V1b input network: the 470 kOhm treble-peaker
        /// resistor against V1b's 1 MOhm grid leak, 1e6/(470e3 + 1e6) (jcm_v1b_cold).
        constexpr float K_V1B_TREBLE_PEAKER_DIVIDER = 0.680272f;

        /// @brief The corner of the treble peaker's shelf, the geometric mean of the network's
        /// zero at 1/(2*pi*470k*470p) = 720.484 Hz and its pole at 1.47 times that, 1059.112 Hz.
        constexpr float K_V1B_TREBLE_PEAKER_HZ = 873.541f;

        /// @brief The peaker's shelf height, which returns the divider above to unity over the
        /// top end: -20*log10(0.680272).
        constexpr float K_V1B_TREBLE_PEAKER_SHELF_DB = 3.34635f;

        /// @brief The flat insertion loss of the passive 2203 tone stack, measured on
        /// FmvToneStack(K_JCM2203_TONE_STACK) at bass = middle = treble = 0.5 and 1 kHz: 0.4030488,
        /// -7.8928 dB, against the -7.84 dB jcm_fmv's own sweep reads (docs/reference/spice/
        /// jcm_fmv). JcmToneStack is unity at flat and does not carry it.
        constexpr float K_TONE_STACK_INSERTION_LOSS = 0.4030488f;

        constexpr float K_BRIGHT_CAP_HZ = 60.0f;
        constexpr float K_COUPLING_CAP_HZ = 120.0f;
        constexpr float K_PRESENCE_HZ = 3000.0f;
        constexpr float K_MAX_PRESENCE_DB = 9.0f;
        constexpr double K_FILTER_Q = 0.707;

        constexpr float K_SMOOTHING_MS = 20.0f;

        // Below this a coefficient rebuild is skipped; see modules/nodes/jcm800/README.md.
        constexpr float K_COEFFICIENT_EPSILON = 1e-4f;

        constexpr float K_MIN_MASTER_DB = -60.0f;
        constexpr float K_MAX_MASTER_DB = 12.0f;
        constexpr float K_MIN_INPUT_TRIM_DB = -24.0f;
        constexpr float K_MAX_INPUT_TRIM_DB = 24.0f;
        constexpr float K_MIN_OUTPUT_DB = -60.0f;
        constexpr float K_MAX_OUTPUT_DB = 12.0f;

        float presence_gain_db(float normalized)
        {
            return (normalized - 0.5f) * 2.0f * K_MAX_PRESENCE_DB;
        }

        /// @brief Returns the corner of one grid's Miller input-capacitance pole.
        /// @param stage_voltage_gain The driven stage's mid-band gain, which is what the
        /// grid-plate capacitance is multiplied by.
        /// @return The -3 dB corner of the grid's RC pole, in Hz.
        double miller_corner_hz(double stage_voltage_gain)
        {
            return K_TRIODE12_AX7_MILLER_CAPACITANCE.corner_hz(stage_voltage_gain,
                                                               K_PLATE_LOAD_OHMS);
        }

        // The knob is an attenuator on a 1.5 audio taper; see modules/nodes/jcm800/README.md.
        float preamp_attenuation(float normalized)
        {
            return audio_taper_pot(normalized) * K_PREAMP_POT_MAX_ATTENUATION;
        }
    } // namespace

    Jcm800::Jcm800()
        : v1a_(K_TRIODE_SOURCE_RESISTANCE, K_TRIODE_GRID_LEAK, K_V1A_CATHODE_BIAS, K_V1A_GAIN,
               K_TRIODE12_AX7_GRID_CONDUCTION, K_V1A_PLATE_SWING),
          v1b_(K_TRIODE_SOURCE_RESISTANCE, K_TRIODE_GRID_LEAK, K_V1B_CATHODE_BIAS, K_V1B_GAIN,
               K_TRIODE12_AX7_GRID_CONDUCTION, K_V1B_PLATE_SWING),
          tone_stack_(48000.0 * K_OVERSAMPLING_FACTOR),
          v2a_(K_TRIODE_SOURCE_RESISTANCE, K_TRIODE_GRID_LEAK, K_V2A_CATHODE_BIAS, K_V2A_GAIN,
               K_TRIODE12_AX7_GRID_CONDUCTION, K_V2A_PLATE_SWING),
          phase_inverter_(K_TRIODE_SOURCE_RESISTANCE, K_TRIODE_GRID_LEAK,
                          K_PHASE_INVERTER.cathode_bias_volts, K_PHASE_INVERTER.gain,
                          K_TRIODE12_AX7_GRID_CONDUCTION, K_PHASE_INVERTER.plate_swing),
          el34_a_(K_EL34), el34_b_(K_EL34),
          output_transformer_(48000.0 * K_OVERSAMPLING_FACTOR, K_OUTPUT_TRANSFORMER_ROLLOFF_HZ,
                              K_OUTPUT_TRANSFORMER_LEAKAGE_HZ,
                              K_OUTPUT_TRANSFORMER_SATURATION_DRIVE),
          sag_(48000.0 * K_OVERSAMPLING_FACTOR, K_SAG_AMOUNT, K_SAG_RECOVERY_TIME_SECONDS)
    {
        power_tube_grid_bias_mismatch_volts_ = K_POWER_TUBE_GRID_BIAS_MISMATCH_VOLTS;

        const double initial_oversampled_rate_hz = 48000.0 * K_OVERSAMPLING_FACTOR;
        v1b_grid_miller_.set_one_pole_low_pass(initial_oversampled_rate_hz,
                                               miller_corner_hz(K_V1B_GAIN));
        v2a_grid_miller_.set_one_pole_low_pass(initial_oversampled_rate_hz,
                                               miller_corner_hz(K_V2A_GAIN));
        phase_inverter_grid_miller_.set_one_pole_low_pass(initial_oversampled_rate_hz,
                                                          miller_corner_hz(K_PHASE_INVERTER.gain));
    }

    void Jcm800::prepare(const PrepareInfo& info)
    {
        sample_rate_ = info.sample_rate;
        bright_cap_.set_high_pass(sample_rate_, K_BRIGHT_CAP_HZ, K_FILTER_Q);
        // The coupling cap is derived at the oversampled rate; see modules/nodes/jcm800/README.md.
        coupling_cap_.set_first_order_high_pass(sample_rate_ * K_OVERSAMPLING_FACTOR,
                                                K_COUPLING_CAP_HZ);
        // The peaker sits inside the oversampled region with the coupling capacitor it follows.
        v1b_treble_peaker_.set_high_shelf(sample_rate_ * K_OVERSAMPLING_FACTOR,
                                          K_V1B_TREBLE_PEAKER_HZ, K_V1B_TREBLE_PEAKER_SHELF_DB,
                                          K_FILTER_Q);
        presence_shelf_.set_high_shelf(sample_rate_, K_PRESENCE_HZ,
                                       presence_gain_db(presence_.target()), K_FILTER_Q);

        // The Miller poles are derived at the oversampled rate; see modules/nodes/jcm800/README.md.
        const double oversampled_rate_hz = sample_rate_ * K_OVERSAMPLING_FACTOR;
        v1b_grid_miller_.set_one_pole_low_pass(oversampled_rate_hz, miller_corner_hz(K_V1B_GAIN));
        v2a_grid_miller_.set_one_pole_low_pass(oversampled_rate_hz, miller_corner_hz(K_V2A_GAIN));
        phase_inverter_grid_miller_.set_one_pole_low_pass(oversampled_rate_hz,
                                                          miller_corner_hz(K_PHASE_INVERTER.gain));

        // The tone stack is built at the oversampled rate; see modules/nodes/jcm800/README.md.
        tone_stack_ = JcmToneStack(sample_rate_ * K_OVERSAMPLING_FACTOR);

        // Every knob smoother snaps to its target here; see modules/nodes/jcm800/README.md.
        SmoothedParameter* const smoothed[] = {&preamp_,          &presence_,   &bass_,
                                               &middle_,          &treble_,     &master_gain_,
                                               &input_trim_gain_, &output_gain_};
        for (SmoothedParameter* parameter : smoothed)
        {
            parameter->prepare(sample_rate_, K_SMOOTHING_MS);
            parameter->snap(parameter->target());
        }
        applied_bass_ = -1.0f;
        applied_middle_ = -1.0f;
        applied_treble_ = -1.0f;
        applied_presence_ = -1.0f;

        // Both stages are built at the oversampled rate; see modules/nodes/jcm800/README.md.
        const double oversampled_rate = sample_rate_ * K_OVERSAMPLING_FACTOR;
        output_transformer_ = OutputTransformerStage(
            oversampled_rate, K_OUTPUT_TRANSFORMER_ROLLOFF_HZ, K_OUTPUT_TRANSFORMER_LEAKAGE_HZ,
            K_OUTPUT_TRANSFORMER_SATURATION_DRIVE);
        sag_ = PowerSupplySagStage(oversampled_rate, K_SAG_AMOUNT, K_SAG_RECOVERY_TIME_SECONDS);

        oversampler_ = std::make_unique<Oversampler8x>(info.max_block);

        scratch_base_.assign(info.max_block, 0.0f);
        scratch_oversampled_.assign(
            static_cast<std::size_t>(info.max_block) * K_OVERSAMPLING_FACTOR, 0.0f);

        reset();
    }

    void Jcm800::process(ProcessContext& context)
    {
        const float* input = context.inputs[0];
        float* output = context.outputs[0];
        const std::uint32_t frames = context.frames;
        const std::uint32_t oversampled_frames = frames * K_OVERSAMPLING_FACTOR;

        // Control changes are only ever targets here; see modules/nodes/jcm800/README.md.
        const float preamp = preamp_.skip(frames);
        const float bass = bass_.skip(frames);
        const float middle = middle_.skip(frames);
        const float treble = treble_.skip(frames);
        const float presence = presence_.skip(frames);
        const float input_gain = input_trim_gain_.skip(frames);
        const float master = master_gain_.skip(frames);
        const float output_gain = output_gain_.skip(frames);

        if (std::fabs(bass - applied_bass_) > K_COEFFICIENT_EPSILON)
        {
            tone_stack_.set_bass(bass);
            applied_bass_ = bass;
        }
        if (std::fabs(middle - applied_middle_) > K_COEFFICIENT_EPSILON)
        {
            tone_stack_.set_middle(middle);
            applied_middle_ = middle;
        }
        if (std::fabs(treble - applied_treble_) > K_COEFFICIENT_EPSILON)
        {
            tone_stack_.set_treble(treble);
            applied_treble_ = treble;
        }
        if (std::fabs(presence - applied_presence_) > K_COEFFICIENT_EPSILON)
        {
            presence_shelf_.set_high_shelf(sample_rate_, K_PRESENCE_HZ, presence_gain_db(presence),
                                           K_FILTER_Q);
            applied_presence_ = presence;
        }

        const float preamp_gain = preamp_attenuation(preamp);
        const float master_gain = master * Jcm800::K_MASTER_UNITY_GAIN;

        for (std::uint32_t i = 0; i < frames; ++i)
        {
            scratch_base_[i] = bright_cap_.process_sample(input[i] * input_gain);
        }

        const float* upsampled = oversampler_->upsample(scratch_base_.data(), frames);
        for (std::uint32_t i = 0; i < oversampled_frames; ++i)
        {
            const float v1a_out = v1a_.process_sample(upsampled[i]);
            const float v1b_in = v1b_grid_miller_.process_sample(v1b_treble_peaker_.process_sample(
                coupling_cap_.process_sample(v1a_out) * preamp_gain * K_V1B_TREBLE_PEAKER_DIVIDER));
            const float v1b_out = v1b_.process_sample(v1b_in);
            const float toned = v2a_grid_miller_.process_sample(
                tone_stack_.process_sample(v1b_out) * K_TONE_STACK_INSERTION_LOSS);
            const float phase_split =
                v2a_.process_sample(toned) / K_PHASE_INVERTER.input_degeneration;
            const float phase_inverted = phase_inverter_.process_sample(
                phase_inverter_grid_miller_.process_sample(phase_split));
            const float el34_a_out = el34_a_.process_sample(phase_inverted);
            const float el34_b_out =
                el34_b_.process_sample(-phase_inverted - power_tube_grid_bias_mismatch_volts_);
            const float power_tube_out =
                (el34_a_out - el34_b_out) * K_POWER_TUBE_TO_TRANSFORMER_ATTENUATION;

            // Both nonlinear stages stay in the 8x region; see modules/nodes/jcm800/README.md.
            const float transformer_out = output_transformer_.process_sample(power_tube_out);
            scratch_oversampled_[i] = sag_.process_sample(transformer_out);
        }
        oversampler_->downsample(scratch_oversampled_.data(), oversampled_frames,
                                 scratch_base_.data());

        // Presence and master run after the decimator; see modules/nodes/jcm800/README.md.
        for (std::uint32_t i = 0; i < frames; ++i)
        {
            scratch_base_[i] = presence_shelf_.process_sample(scratch_base_[i]) * master_gain;
        }

        // Cabinet convolution is the sushidsp.cabinet node's; see modules/nodes/jcm800/README.md.
        for (std::uint32_t i = 0; i < frames; ++i)
        {
            output[i] = scratch_base_[i];
        }

        // The clamp is a safety rail for the DAC's range; see modules/nodes/jcm800/README.md.
        apply_output_safety_clamp(output, frames, output_gain);
    }

    std::uint32_t Jcm800::latency_samples() const
    {
        return oversampler_ ? oversampler_->latency_samples() : 0;
    }

    const char* Jcm800::type_id() const
    {
        return "sushidsp.jcm800";
    }

    PortLayout Jcm800::ports() const
    {
        return {1, 1};
    }

    void Jcm800::reset()
    {
        bright_cap_.reset();
        coupling_cap_.reset();
        v1b_treble_peaker_.reset();
        presence_shelf_.reset();
        v1b_grid_miller_.reset();
        v2a_grid_miller_.reset();
        phase_inverter_grid_miller_.reset();
        v1a_.reset();
        v1b_.reset();
        tone_stack_.reset();
        v2a_.reset();
        phase_inverter_.reset();
        el34_a_.reset();
        el34_b_.reset();
        output_transformer_.reset();
        sag_.reset();
        if (oversampler_)
        {
            oversampler_->reset();
        }
    }

    void Jcm800::set_preamp(float normalized)
    {
        preamp_.set_target(std::clamp(normalized, 0.0f, 1.0f));
    }
    void Jcm800::set_bass(float normalized)
    {
        bass_.set_target(std::clamp(normalized, 0.0f, 1.0f));
    }
    void Jcm800::set_middle(float normalized)
    {
        middle_.set_target(std::clamp(normalized, 0.0f, 1.0f));
    }
    void Jcm800::set_treble(float normalized)
    {
        treble_.set_target(std::clamp(normalized, 0.0f, 1.0f));
    }
    void Jcm800::set_presence(float normalized)
    {
        presence_.set_target(std::clamp(normalized, 0.0f, 1.0f));
    }

    void Jcm800::set_power_tube_grid_bias_mismatch_volts(float volts)
    {
        power_tube_grid_bias_mismatch_volts_ = volts;
    }

    std::size_t Jcm800::triode_stage_count() const
    {
        return 4;
    }

    float Jcm800::grid_excursion_peak_volts(std::size_t stage) const
    {
        const TriodeStage* const stages[] = {&v1a_, &v1b_, &v2a_, &phase_inverter_};
        if (stage >= triode_stage_count())
        {
            return 0.0f;
        }
        return stages[stage]->peak_grid_excursion_volts();
    }

    void Jcm800::set_master(float decibels)
    {
        master_db_ = std::clamp(decibels, K_MIN_MASTER_DB, K_MAX_MASTER_DB);
        master_gain_.set_target(decibels_to_gain(master_db_));
    }

    void Jcm800::set_input_trim(float decibels)
    {
        input_trim_db_ = std::clamp(decibels, K_MIN_INPUT_TRIM_DB, K_MAX_INPUT_TRIM_DB);
        input_trim_gain_.set_target(decibels_to_gain(input_trim_db_));
    }

    void Jcm800::set_output(float decibels)
    {
        output_db_ = std::clamp(decibels, K_MIN_OUTPUT_DB, K_MAX_OUTPUT_DB);
        output_gain_.set_target(decibels_to_gain(output_db_));
    }

    bool Jcm800::tone_stack_is_dirty() const
    {
        return std::fabs(bass_.target() - applied_bass_) > K_COEFFICIENT_EPSILON ||
               std::fabs(middle_.target() - applied_middle_) > K_COEFFICIENT_EPSILON ||
               std::fabs(treble_.target() - applied_treble_) > K_COEFFICIENT_EPSILON ||
               std::fabs(presence_.target() - applied_presence_) > K_COEFFICIENT_EPSILON;
    }

    const ParameterTable<Jcm800, 8>& Jcm800::parameters()
    {
        static const ParameterTable<Jcm800, 8> table{{{
            {{"preamp", "Preamp", 0.0f, 1.0f, 0.5f, ParamKind::Linear, ""},
             [](Jcm800& n, float v)
             {
                 n.set_preamp(v);
             },
             [](const Jcm800& n)
             {
                 return n.preamp_.target();
             }},
            {{"master", "Master", K_MIN_MASTER_DB, K_MAX_MASTER_DB, 0.0f, ParamKind::Decibel, "dB"},
             [](Jcm800& n, float v)
             {
                 n.set_master(v);
             },
             [](const Jcm800& n)
             {
                 return n.master_db_;
             }},
            {{"bass", "Bass", 0.0f, 1.0f, 0.5f, ParamKind::Linear, ""},
             [](Jcm800& n, float v)
             {
                 n.set_bass(v);
             },
             [](const Jcm800& n)
             {
                 return n.bass_.target();
             }},
            {{"middle", "Middle", 0.0f, 1.0f, 0.5f, ParamKind::Linear, ""},
             [](Jcm800& n, float v)
             {
                 n.set_middle(v);
             },
             [](const Jcm800& n)
             {
                 return n.middle_.target();
             }},
            {{"treble", "Treble", 0.0f, 1.0f, 0.5f, ParamKind::Linear, ""},
             [](Jcm800& n, float v)
             {
                 n.set_treble(v);
             },
             [](const Jcm800& n)
             {
                 return n.treble_.target();
             }},
            {{"presence", "Presence", 0.0f, 1.0f, 0.5f, ParamKind::Linear, ""},
             [](Jcm800& n, float v)
             {
                 n.set_presence(v);
             },
             [](const Jcm800& n)
             {
                 return n.presence_.target();
             }},
            {{"input_trim", "Input Trim", K_MIN_INPUT_TRIM_DB, K_MAX_INPUT_TRIM_DB, 0.0f,
              ParamKind::Decibel, "dB"},
             [](Jcm800& n, float v)
             {
                 n.set_input_trim(v);
             },
             [](const Jcm800& n)
             {
                 return n.input_trim_db_;
             }},
            {{"output", "Output", K_MIN_OUTPUT_DB, K_MAX_OUTPUT_DB, 0.0f, ParamKind::Decibel, "dB"},
             [](Jcm800& n, float v)
             {
                 n.set_output(v);
             },
             [](const Jcm800& n)
             {
                 return n.output_db_;
             }},
        }}};
        return table;
    }

    float Jcm800::get_parameter(const char* id) const
    {
        return parameters().read(*this, id);
    }

    void Jcm800::set_parameter_for_control_thread(const char* id, float value)
    {
        parameters().write(*this, id, value);
    }

    void Jcm800::set_parameter_for_audio_thread(std::uint16_t index, float value)
    {
        parameters().write(*this, index, value);
    }

    namespace
    {
        std::unique_ptr<AudioNode> make_jcm800(const PrepareInfo&)
        {
            return std::make_unique<Jcm800>();
        }
    } // namespace

    const NodeDescriptor& jcm800_descriptor()
    {
        static const NodeDescriptor descriptor{
            "sushidsp.jcm800",
            "JCM800",
            Jcm800::parameters().descriptors(),
            Jcm800::parameters().count(),
            PortLayout{1, 1},
            &make_jcm800,
        };
        return descriptor;
    }
} // namespace SushiDSP
