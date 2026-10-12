/**************************************************************************/
/* diezel_vh4.cpp                                                         */
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
 * @file diezel_vh4.cpp
 * @brief The VH4 channel-3 chain, its constants and its descriptor.
 * @author Mustafa Garip & Sushi Systems
 */

// _USE_MATH_DEFINES must precede every include: it is what makes MSVC's <cmath> declare M_PI.
#define _USE_MATH_DEFINES

#include "SushiDSP/diezel_vh4/diezel_vh4.hpp"

#include "SushiDSP/circuits/output_safety_clamp.hpp"
#include "SushiDSP/circuits/stages/phase_inverter_presets.hpp"
#include "SushiDSP/circuits/stages/power_tube_presets.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace SushiDSP
{
    namespace
    {
        /// @brief Diezel VH4, EL34 quad at 500 V: the Philips EL34 datasheet's 500 V push-pull point
        /// (Vg1 -36 V, Raa 4 kOhm, 70 W) at Diezel's 35 mA idle (modules/nodes/diezel_vh4/README.md, "The
        /// power tube is an EL34").
        constexpr PowerTubePreset K_DIEZEL_VH4_POWER_EL34{10000.0f,
                                                          220000.0f,
                                                          -36.0f,
                                                          8.458480f,
                                                          K_PENTODE_EL34_GRID_CONDUCTION,
                                                          PlateSwing{35.0f, 339.1657f}};

        // Each "Section" below is one of the VH4 dossier's; see modules/nodes/diezel_vh4/README.md.

        // Section 3.1: R1 1 MOhm; C1 is not modelled; see modules/nodes/diezel_vh4/README.md.

        // Section 3.1: V1a runs Ra 120 kOhm and Rk 2k2 fully bypassed by C2 47 uF, zero at 1.5 Hz.
        constexpr float K_V1A_GRID_LEAK_OHMS = 1000000.0f;
        constexpr float K_V1A_CATHODE_BIAS_VOLTS = -2.05f;
        // Section 6: x61.4 unloaded, x56.8 into the 470 kOhm the channel-3 leg presents.
        constexpr float K_V1A_GAIN = 56.8f;
        // Section 3.1 draws no stopper; 10 kOhm stands in; see modules/nodes/diezel_vh4/README.md.
        constexpr float K_V1A_SOURCE_RESISTANCE_OHMS = 10000.0f;
        /// @brief V1a's plate swing: 367 - 255.3 = 111.7 V to the supply, and 255.3 - 111.133 =
        /// 144.167 V to the bottom, the plate solved at Vgk = 0 on the 120 kOhm load line with the
        /// Koren 12AX7 (2.132 mA) above the 2.05 V cathode.
        constexpr PlateSwing K_V1A_PLATE_SWING{111.700f, 144.167f};

        // Sections 3.1, 6: Ra 120 kOhm || rp 54.6 kOhm; see modules/nodes/diezel_vh4/README.md.
        constexpr double K_V1A_PLATE_IMPEDANCE_OHMS = 37530.0;
        constexpr double K_GAIN_COUPLING_CAPACITOR_FARADS = 1e-9;  // Section 3.1: C13.
        constexpr double K_GAIN_SERIES_RESISTANCE_OHMS = 220000.0; // Section 3.1: R14.
        constexpr double K_GAIN_POTENTIOMETER_OHMS = 250000.0;     // Section 3.2: the GAIN pot.

        // Section 3.3: R21 47 kOhm grid stopper, R22 3M3 grid leak as drawn, Vgk -1.02 V at 367 V.
        constexpr float K_V2A_SOURCE_RESISTANCE_OHMS = 47000.0f;
        constexpr float K_V2A_GRID_LEAK_OHMS = 3300000.0f;
        constexpr float K_V2A_CATHODE_BIAS_VOLTS = -1.02f;
        constexpr float K_V2A_GAIN = 67.2f; // Section 6: x67.2 into the 500 kOhm shunt.
        /// @brief V2a's plate swing: 367 - 186.2 = 180.8 V to the supply, and 186.2 - 110.343 =
        /// 75.857 V to the bottom, the plate solved at Vgk = 0 on the 120 kOhm load line with the
        /// Koren 12AX7 (2.139 mA) above the 1.02 V cathode.
        constexpr PlateSwing K_V2A_PLATE_SWING{180.800f, 75.857f};
        // Sections 3.3, 6: Rk 680 Ohm with only C17 1 uF across it, 8.22 dB of loss below the zero.
        constexpr double K_V2A_CATHODE_SHELF_ZERO_HZ = 234.0;
        constexpr double K_V2A_CATHODE_SHELF_POLE_HZ = 603.0;

        // C13, C18 and C20 are first-order high-passes; see modules/nodes/diezel_vh4/README.md.

        // Section 6: C18 4.7 nF into V2a's 30.8 kOhm plate impedance plus the 500 kOhm shunt.
        constexpr double K_V3A_COUPLING_HIGH_PASS_HZ = 63.8;
        // Section 6: the relay node's 500 kOhm against 500 kOhm + 30.8 kOhm + R26 470 kOhm.
        constexpr float K_V2A_TO_V3A_DIVIDER = 0.4996003f;

        // Section 3.4 gives V3a's five values; see modules/nodes/diezel_vh4/README.md.
        constexpr float K_V3A_SOURCE_RESISTANCE_OHMS = 470000.0f;
        constexpr float K_V3A_GRID_LEAK_OHMS = 500000.0f;
        constexpr float K_V3A_CATHODE_BIAS_VOLTS = -1.02f;
        constexpr float K_V3A_GAIN = 71.4f; // Section 6: x71.4.
        /// @brief V3a's plate swing, the same point V2a sits at: 367 - 186.2 = 180.8 V to the
        /// supply, and 186.2 - 110.343 = 75.857 V to the bottom, solved at Vgk = 0 on the 120 kOhm
        /// load line above the 1.02 V cathode.
        constexpr PlateSwing K_V3A_PLATE_SWING{180.800f, 75.857f};
        constexpr double K_V3A_CATHODE_SHELF_ZERO_HZ = 106.0;
        constexpr double K_V3A_CATHODE_SHELF_POLE_HZ = 274.0;

        // Sections 3.5 and 6: the DC-coupled follower's gain is 0.986, its output corner 2.26 Hz.
        constexpr float K_CATHODE_FOLLOWER_GAIN = 0.986f;
        constexpr double K_FOLLOWER_HIGH_PASS_HZ = 2.26;

        // Sections 2 and 4: R42 470 kOhm into the 470 kOhm volume fixes a 470/(470+470) divider.
        constexpr float K_VOLUME_SERIES_DIVIDER = 0.5f;
        // Section 6: ch3 selected, R43 680 kOhm into the mix node's 48.1 kOhm, x0.0660, -23.6 dB.
        constexpr float K_MIX_BUS_ATTENUATION = 0.0660f;

        // Sections 2 and 6: V4a's grid is the mix node, which sets both resistances below.
        constexpr float K_V4A_SOURCE_RESISTANCE_OHMS = 44923.0f;
        constexpr float K_V4A_GRID_LEAK_OHMS = 48100.0f;
        constexpr float K_V4A_CATHODE_BIAS_VOLTS = -0.98f;
        constexpr float K_V4A_GAIN = 28.7f;
        /// @brief V4a's plate swing: 367 - 183.0 = 184.0 V to the supply, and 183.0 - 110.312 =
        /// 72.688 V to the bottom, the plate solved at Vgk = 0 on the 120 kOhm load line with the
        /// Koren 12AX7 (2.139 mA) above the 0.98 V cathode. The 640 Ohm cathode is unbypassed, so
        /// Vgk = 0 is read as the conduction limit at the idle cathode voltage; the README says so.
        constexpr PlateSwing K_V4A_PLATE_SWING{184.000f, 72.688f};
        // V4b reuses V3b's 0.986 follower gain, and the send pad is not applied.

        // Section 5 sets nothing past the send jack; this is the corpus's 82 kOhm operating point.
        constexpr float K_PHASE_INVERTER_SOURCE_RESISTANCE_OHMS = 10000.0f;
        constexpr float K_PHASE_INVERTER_GRID_LEAK_OHMS = 1000000.0f;

        /// @brief The pair this chain stands in for, derived in
        /// modules/foundation/circuits/include/SushiDSP/circuits/stages/phase_inverter_presets.hpp.
        constexpr SingleTriodeInverter K_PHASE_INVERTER = K_TRIODE12_AX7_INVERTER82K_PLATE;

        /// @brief The level the output transformer stage receives at the pair's full clip: the
        /// 2 x 22.222 V x 0.05 this chain delivered before PentodeStage became volt-true. The
        /// unit the transformer stage works in is a corpus convention, held across every
        /// amplifier so one transformer model serves them all.
        /// @see modules/nodes/diezel_vh4/README.md, "Level staging is gone"
        constexpr float K_TRANSFORMER_FULL_CLIP_UNITS = 2.222222f;

        /// @brief The output trim from the EL34 plates to the transformer's units, this chain's
        /// one output trim: the pair's full swing, K_DIEZEL_VH4_POWER_EL34's two headrooms summed,
        /// lands at K_TRANSFORMER_FULL_CLIP_UNITS.
        constexpr float K_POWER_TUBE_TO_TRANSFORMER_ATTENUATION =
            K_TRANSFORMER_FULL_CLIP_UNITS /
            (K_DIEZEL_VH4_POWER_EL34.plate_swing.headroom_to_supply_volts +
             K_DIEZEL_VH4_POWER_EL34.plate_swing.headroom_to_bottom_volts);

        // Section 5 establishes none of the five figures below; they are the corpus's stand-ins.
        constexpr float K_OUTPUT_TRANSFORMER_ROLLOFF_HZ = 60.0f;
        constexpr float K_OUTPUT_TRANSFORMER_LEAKAGE_HZ = 8000.0f;
        constexpr float K_OUTPUT_TRANSFORMER_SATURATION_DRIVE = 0.3f;
        constexpr float K_SAG_AMOUNT = 0.3f;
        constexpr float K_SAG_RECOVERY_TIME_SECONDS = 0.05f;

        // Sections 5 and 6: Presence centres at about 4 kHz (S2); its 9 dB depth is a choice.
        constexpr float K_PRESENCE_HZ = 4000.0f;
        constexpr float K_MAX_PRESENCE_DB = 9.0f;
        // Sections 5 and 6: Deep centres at 80 Hz (S2); its 10 dB first-order shelf is a choice.
        constexpr float K_DEEP_HZ = 80.0f;
        constexpr float K_MAX_DEEP_BOOST_DB = 10.0f;

        constexpr double K_FILTER_Q = 0.707;

        // 12AX7 Miller input capacitance: Cgk 1.6 pF, Cgp 1.7 pF and 10 pF of stray wiring.
        constexpr double K_MILLER_CGK_PICOFARADS = 1.6;
        constexpr double K_MILLER_CGP_PICOFARADS = 1.7;
        constexpr double K_STRAY_WIRING_PICOFARADS = 10.0;

        constexpr float K_SMOOTHING_MS = 20.0f;
        constexpr float K_COEFFICIENT_EPSILON = 1e-4f;

        constexpr float K_MIN_MASTER_DB = -60.0f;
        constexpr float K_MAX_MASTER_DB = 12.0f;
        constexpr float K_MIN_INPUT_TRIM_DB = -24.0f;
        constexpr float K_MAX_INPUT_TRIM_DB = 24.0f;
        constexpr float K_MIN_OUTPUT_DB = -60.0f;
        constexpr float K_MAX_OUTPUT_DB = 12.0f;

        /// @brief The corner of one driven grid's Miller input-capacitance pole.
        /// @param stage_voltage_gain The driven stage's midband gain.
        /// @param driving_resistance_ohms The resistance the grid is driven through.
        /// @return The -3 dB corner in Hz.
        double miller_corner_hz(double stage_voltage_gain, double driving_resistance_ohms)
        {
            const double input_capacitance_farads =
                (K_MILLER_CGK_PICOFARADS + K_MILLER_CGP_PICOFARADS * (1.0 + stage_voltage_gain) +
                 K_STRAY_WIRING_PICOFARADS) *
                1e-12;
            return 1.0 / (2.0 * M_PI * driving_resistance_ohms * input_capacitance_farads);
        }

        /// @brief The total resistance C13 works against for one gain-knob position.
        /// @param normalized The gain knob position in [0, 1].
        /// @return V1a's plate impedance plus R14 plus the pot's shunt leg, in ohms.
        double gain_network_resistance_ohms(double normalized)
        {
            return K_V1A_PLATE_IMPEDANCE_OHMS + K_GAIN_SERIES_RESISTANCE_OHMS +
                   normalized * K_GAIN_POTENTIOMETER_OHMS;
        }

        /// @brief The gain pot's own division ratio.
        /// @param normalized The gain knob position in [0, 1].
        /// @return The fraction of V1a's plate signal that reaches R21.
        float gain_attenuation(float normalized)
        {
            const double total = gain_network_resistance_ohms(normalized);
            return static_cast<float>(normalized * K_GAIN_POTENTIOMETER_OHMS / total);
        }

        /// @brief Presence in decibels for a knob position.
        /// @param normalized Presence knob position in [0, 1]; 0.5 is flat.
        /// @return The shelf gain in decibels.
        float presence_gain_db(float normalized)
        {
            return (normalized - 0.5f) * 2.0f * K_MAX_PRESENCE_DB;
        }
    } // namespace

    DiezelVh4::DiezelVh4()
        : v1a_(K_V1A_SOURCE_RESISTANCE_OHMS, K_V1A_GRID_LEAK_OHMS, K_V1A_CATHODE_BIAS_VOLTS,
               K_V1A_GAIN, K_TRIODE12_AX7_GRID_CONDUCTION, K_V1A_PLATE_SWING),
          v2a_(K_V2A_SOURCE_RESISTANCE_OHMS, K_V2A_GRID_LEAK_OHMS, K_V2A_CATHODE_BIAS_VOLTS,
               K_V2A_GAIN, K_TRIODE12_AX7_GRID_CONDUCTION, K_V2A_PLATE_SWING),
          v3a_(K_V3A_SOURCE_RESISTANCE_OHMS, K_V3A_GRID_LEAK_OHMS, K_V3A_CATHODE_BIAS_VOLTS,
               K_V3A_GAIN, K_TRIODE12_AX7_GRID_CONDUCTION, K_V3A_PLATE_SWING),
          tone_stack_(48000.0 * K_OVERSAMPLING_FACTOR),
          v4a_(K_V4A_SOURCE_RESISTANCE_OHMS, K_V4A_GRID_LEAK_OHMS, K_V4A_CATHODE_BIAS_VOLTS,
               K_V4A_GAIN, K_TRIODE12_AX7_GRID_CONDUCTION, K_V4A_PLATE_SWING),
          phase_inverter_(K_PHASE_INVERTER_SOURCE_RESISTANCE_OHMS, K_PHASE_INVERTER_GRID_LEAK_OHMS,
                          K_PHASE_INVERTER.cathode_bias_volts, K_PHASE_INVERTER.gain,
                          K_TRIODE12_AX7_GRID_CONDUCTION, K_PHASE_INVERTER.plate_swing),
          el34_a_(K_DIEZEL_VH4_POWER_EL34), el34_b_(K_DIEZEL_VH4_POWER_EL34),
          output_transformer_(48000.0 * K_OVERSAMPLING_FACTOR, K_OUTPUT_TRANSFORMER_ROLLOFF_HZ,
                              K_OUTPUT_TRANSFORMER_LEAKAGE_HZ,
                              K_OUTPUT_TRANSFORMER_SATURATION_DRIVE),
          sag_(48000.0 * K_OVERSAMPLING_FACTOR, K_SAG_AMOUNT, K_SAG_RECOVERY_TIME_SECONDS),
          deep_shelf_(48000.0, K_DEEP_HZ, K_MAX_DEEP_BOOST_DB)
    {
        rebuild_gain_high_pass(gain_.target());
    }

    void DiezelVh4::prepare(const PrepareInfo& info)
    {
        sample_rate_ = info.sample_rate;
        const double oversampled_rate = sample_rate_ * K_OVERSAMPLING_FACTOR;

        presence_shelf_.set_high_shelf(sample_rate_, K_PRESENCE_HZ,
                                       presence_gain_db(presence_.target()), K_FILTER_Q);
        deep_shelf_ = ResonanceControl(sample_rate_, K_DEEP_HZ, K_MAX_DEEP_BOOST_DB);
        deep_shelf_.set_depth(deep_.target());

        // Each corner from the gain pot's capacitor to the sag stage is derived at 8x host rate.
        rebuild_gain_high_pass(gain_.target());
        v2a_cathode_shelf_.set_first_order_shelf(
            oversampled_rate, K_V2A_CATHODE_SHELF_ZERO_HZ, K_V2A_CATHODE_SHELF_POLE_HZ,
            K_V2A_CATHODE_SHELF_ZERO_HZ / K_V2A_CATHODE_SHELF_POLE_HZ);
        v2a_grid_miller_.set_one_pole_low_pass(
            oversampled_rate, miller_corner_hz(K_V2A_GAIN, K_V2A_SOURCE_RESISTANCE_OHMS));
        v3a_coupling_high_pass_.set_first_order_high_pass(oversampled_rate,
                                                          K_V3A_COUPLING_HIGH_PASS_HZ);
        v3a_cathode_shelf_.set_first_order_shelf(
            oversampled_rate, K_V3A_CATHODE_SHELF_ZERO_HZ, K_V3A_CATHODE_SHELF_POLE_HZ,
            K_V3A_CATHODE_SHELF_ZERO_HZ / K_V3A_CATHODE_SHELF_POLE_HZ);
        v3a_grid_miller_.set_one_pole_low_pass(
            oversampled_rate, miller_corner_hz(K_V3A_GAIN, K_V3A_SOURCE_RESISTANCE_OHMS));
        follower_high_pass_.set_first_order_high_pass(oversampled_rate, K_FOLLOWER_HIGH_PASS_HZ);

        tone_stack_ = DiezelVh4ToneStack(oversampled_rate);
        output_transformer_ = OutputTransformerStage(
            oversampled_rate, K_OUTPUT_TRANSFORMER_ROLLOFF_HZ, K_OUTPUT_TRANSFORMER_LEAKAGE_HZ,
            K_OUTPUT_TRANSFORMER_SATURATION_DRIVE);
        sag_ = PowerSupplySagStage(oversampled_rate, K_SAG_AMOUNT, K_SAG_RECOVERY_TIME_SECONDS);

        SmoothedParameter* const smoothed[] = {
            &gain_, &bass_,     &middle_,      &treble_,          &volume_,
            &deep_, &presence_, &master_gain_, &input_trim_gain_, &output_gain_};
        for (SmoothedParameter* parameter : smoothed)
        {
            parameter->prepare(sample_rate_, K_SMOOTHING_MS);
            parameter->snap(parameter->target());
        }
        applied_gain_ = -1.0f;
        applied_bass_ = -1.0f;
        applied_middle_ = -1.0f;
        applied_treble_ = -1.0f;
        applied_deep_ = -1.0f;
        applied_presence_ = -1.0f;

        oversampler_ = std::make_unique<Oversampler8x>(info.max_block);

        scratch_base_.assign(info.max_block, 0.0f);
        scratch_oversampled_.assign(
            static_cast<std::size_t>(info.max_block) * K_OVERSAMPLING_FACTOR, 0.0f);

        reset();
    }

    void DiezelVh4::rebuild_gain_high_pass(float normalized)
    {
        const double corner_hz = 1.0 / (2.0 * M_PI * K_GAIN_COUPLING_CAPACITOR_FARADS *
                                        gain_network_resistance_ohms(normalized));
        gain_high_pass_.set_first_order_high_pass(sample_rate_ * K_OVERSAMPLING_FACTOR, corner_hz);
    }

    void DiezelVh4::process(ProcessContext& context)
    {
        const float* input = context.inputs[0];
        float* output = context.outputs[0];
        const std::uint32_t frames = context.frames;
        const std::uint32_t oversampled_frames = frames * K_OVERSAMPLING_FACTOR;

        const float gain = gain_.skip(frames);
        const float bass = bass_.skip(frames);
        const float middle = middle_.skip(frames);
        const float treble = treble_.skip(frames);
        const float volume = volume_.skip(frames);
        const float deep = deep_.skip(frames);
        const float presence = presence_.skip(frames);
        const float input_gain = input_trim_gain_.skip(frames);
        const float master = master_gain_.skip(frames);
        const float output_gain = output_gain_.skip(frames);

        if (std::fabs(gain - applied_gain_) > K_COEFFICIENT_EPSILON)
        {
            rebuild_gain_high_pass(gain);
            applied_gain_ = gain;
        }
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
        if (std::fabs(deep - applied_deep_) > K_COEFFICIENT_EPSILON)
        {
            deep_shelf_.set_depth(deep);
            applied_deep_ = deep;
        }
        if (std::fabs(presence - applied_presence_) > K_COEFFICIENT_EPSILON)
        {
            presence_shelf_.set_high_shelf(sample_rate_, K_PRESENCE_HZ, presence_gain_db(presence),
                                           K_FILTER_Q);
            applied_presence_ = presence;
        }

        const float gain_pot = gain_attenuation(gain);
        const float volume_pot = volume * K_VOLUME_SERIES_DIVIDER;
        const float master_gain = master * DiezelVh4::K_MASTER_UNITY_GAIN;

        for (std::uint32_t i = 0; i < frames; ++i)
        {
            scratch_base_[i] = input[i] * input_gain;
        }

        const float* upsampled = oversampler_->upsample(scratch_base_.data(), frames);
        for (std::uint32_t i = 0; i < oversampled_frames; ++i)
        {
            const float v1a_out = v1a_.process_sample(upsampled[i]);
            const float v2a_in = v2a_grid_miller_.process_sample(v2a_cathode_shelf_.process_sample(
                gain_high_pass_.process_sample(v1a_out) * gain_pot));
            const float v2a_out =
                v3a_coupling_high_pass_.process_sample(v2a_.process_sample(v2a_in)) *
                K_V2A_TO_V3A_DIVIDER;
            const float v3a_in =
                v3a_grid_miller_.process_sample(v3a_cathode_shelf_.process_sample(v2a_out));
            const float follower_out = follower_high_pass_.process_sample(
                v3a_.process_sample(v3a_in) * K_CATHODE_FOLLOWER_GAIN);
            const float mix_bus =
                tone_stack_.process_sample(follower_out) * volume_pot * K_MIX_BUS_ATTENUATION;
            const float send_buffer = v4a_.process_sample(mix_bus) * K_CATHODE_FOLLOWER_GAIN;
            const float phase_inverted =
                phase_inverter_.process_sample(send_buffer / K_PHASE_INVERTER.input_degeneration);
            const float el34_a_out = el34_a_.process_sample(phase_inverted);
            const float el34_b_out = el34_b_.process_sample(-phase_inverted);
            const float power_tube_out =
                (el34_a_out - el34_b_out) * K_POWER_TUBE_TO_TRANSFORMER_ATTENUATION;

            const float transformer_out = output_transformer_.process_sample(power_tube_out);
            scratch_oversampled_[i] = sag_.process_sample(transformer_out);
        }
        oversampler_->downsample(scratch_oversampled_.data(), oversampled_frames,
                                 scratch_base_.data());

        // Deep and Presence are linear, so they run after the decimator with the master volume.
        for (std::uint32_t i = 0; i < frames; ++i)
        {
            const float presence_out = presence_shelf_.process_sample(scratch_base_[i]);
            scratch_base_[i] = deep_shelf_.process_sample(presence_out) * master_gain;
        }

        // Cabinet convolution is the sushidsp.cabinet node's; the dry signal goes straight out.
        for (std::uint32_t i = 0; i < frames; ++i)
        {
            output[i] = scratch_base_[i];
        }

        // The clamp is a safety rail for the DAC's range; see modules/nodes/diezel_vh4/README.md.
        apply_output_safety_clamp(output, frames, output_gain);
    }

    std::uint32_t DiezelVh4::latency_samples() const
    {
        return oversampler_ ? oversampler_->latency_samples() : 0;
    }

    const char* DiezelVh4::type_id() const
    {
        return "sushidsp.diezel_vh4";
    }

    std::size_t DiezelVh4::triode_stage_count() const
    {
        return 5;
    }

    float DiezelVh4::grid_excursion_peak_volts(std::size_t stage) const
    {
        const TriodeStage* const stages[] = {&v1a_, &v2a_, &v3a_, &v4a_, &phase_inverter_};
        if (stage >= triode_stage_count())
        {
            return 0.0f;
        }
        return stages[stage]->peak_grid_excursion_volts();
    }

    PortLayout DiezelVh4::ports() const
    {
        return {1, 1};
    }

    void DiezelVh4::reset()
    {
        gain_high_pass_.reset();
        v2a_cathode_shelf_.reset();
        v2a_grid_miller_.reset();
        v3a_coupling_high_pass_.reset();
        v3a_cathode_shelf_.reset();
        v3a_grid_miller_.reset();
        follower_high_pass_.reset();
        presence_shelf_.reset();
        v1a_.reset();
        v2a_.reset();
        v3a_.reset();
        tone_stack_.reset();
        v4a_.reset();
        phase_inverter_.reset();
        el34_a_.reset();
        el34_b_.reset();
        output_transformer_.reset();
        sag_.reset();
        deep_shelf_.reset();
        if (oversampler_)
        {
            oversampler_->reset();
        }
    }

    void DiezelVh4::set_gain(float normalized)
    {
        gain_.set_target(std::clamp(normalized, 0.0f, 1.0f));
    }
    void DiezelVh4::set_bass(float normalized)
    {
        bass_.set_target(std::clamp(normalized, 0.0f, 1.0f));
    }
    void DiezelVh4::set_middle(float normalized)
    {
        middle_.set_target(std::clamp(normalized, 0.0f, 1.0f));
    }
    void DiezelVh4::set_treble(float normalized)
    {
        treble_.set_target(std::clamp(normalized, 0.0f, 1.0f));
    }
    void DiezelVh4::set_volume(float normalized)
    {
        volume_.set_target(std::clamp(normalized, 0.0f, 1.0f));
    }
    void DiezelVh4::set_deep(float normalized)
    {
        deep_.set_target(std::clamp(normalized, 0.0f, 1.0f));
    }
    void DiezelVh4::set_presence(float normalized)
    {
        presence_.set_target(std::clamp(normalized, 0.0f, 1.0f));
    }

    void DiezelVh4::set_master(float decibels)
    {
        master_db_ = std::clamp(decibels, K_MIN_MASTER_DB, K_MAX_MASTER_DB);
        master_gain_.set_target(decibels_to_gain(master_db_));
    }

    void DiezelVh4::set_input_trim(float decibels)
    {
        input_trim_db_ = std::clamp(decibels, K_MIN_INPUT_TRIM_DB, K_MAX_INPUT_TRIM_DB);
        input_trim_gain_.set_target(decibels_to_gain(input_trim_db_));
    }

    void DiezelVh4::set_output(float decibels)
    {
        output_db_ = std::clamp(decibels, K_MIN_OUTPUT_DB, K_MAX_OUTPUT_DB);
        output_gain_.set_target(decibels_to_gain(output_db_));
    }

    bool DiezelVh4::tone_stack_is_dirty() const
    {
        return std::fabs(gain_.target() - applied_gain_) > K_COEFFICIENT_EPSILON ||
               std::fabs(bass_.target() - applied_bass_) > K_COEFFICIENT_EPSILON ||
               std::fabs(middle_.target() - applied_middle_) > K_COEFFICIENT_EPSILON ||
               std::fabs(treble_.target() - applied_treble_) > K_COEFFICIENT_EPSILON ||
               std::fabs(deep_.target() - applied_deep_) > K_COEFFICIENT_EPSILON ||
               std::fabs(presence_.target() - applied_presence_) > K_COEFFICIENT_EPSILON;
    }

    const ParameterTable<DiezelVh4, 10>& DiezelVh4::parameters()
    {
        // Row order is a contract; rows are only appended; see modules/nodes/diezel_vh4/README.md.
        static const ParameterTable<DiezelVh4, 10> table{{{
            {{"gain", "Gain", 0.0f, 1.0f, 0.5f, ParamKind::Linear, ""},
             [](DiezelVh4& n, float v)
             {
                 n.set_gain(v);
             },
             [](const DiezelVh4& n)
             {
                 return n.gain_.target();
             }},
            {{"bass", "Bass", 0.0f, 1.0f, 0.5f, ParamKind::Linear, ""},
             [](DiezelVh4& n, float v)
             {
                 n.set_bass(v);
             },
             [](const DiezelVh4& n)
             {
                 return n.bass_.target();
             }},
            {{"middle", "Middle", 0.0f, 1.0f, 0.5f, ParamKind::Linear, ""},
             [](DiezelVh4& n, float v)
             {
                 n.set_middle(v);
             },
             [](const DiezelVh4& n)
             {
                 return n.middle_.target();
             }},
            {{"treble", "Treble", 0.0f, 1.0f, 0.5f, ParamKind::Linear, ""},
             [](DiezelVh4& n, float v)
             {
                 n.set_treble(v);
             },
             [](const DiezelVh4& n)
             {
                 return n.treble_.target();
             }},
            {{"volume", "Volume", 0.0f, 1.0f, 0.5f, ParamKind::Linear, ""},
             [](DiezelVh4& n, float v)
             {
                 n.set_volume(v);
             },
             [](const DiezelVh4& n)
             {
                 return n.volume_.target();
             }},
            {{"deep", "Deep", 0.0f, 1.0f, 0.5f, ParamKind::Linear, ""},
             [](DiezelVh4& n, float v)
             {
                 n.set_deep(v);
             },
             [](const DiezelVh4& n)
             {
                 return n.deep_.target();
             }},
            {{"presence", "Presence", 0.0f, 1.0f, 0.5f, ParamKind::Linear, ""},
             [](DiezelVh4& n, float v)
             {
                 n.set_presence(v);
             },
             [](const DiezelVh4& n)
             {
                 return n.presence_.target();
             }},
            {{"master", "Master", K_MIN_MASTER_DB, K_MAX_MASTER_DB, 0.0f, ParamKind::Decibel, "dB"},
             [](DiezelVh4& n, float v)
             {
                 n.set_master(v);
             },
             [](const DiezelVh4& n)
             {
                 return n.master_db_;
             }},
            {{"input_trim", "Input Trim", K_MIN_INPUT_TRIM_DB, K_MAX_INPUT_TRIM_DB, 0.0f,
              ParamKind::Decibel, "dB"},
             [](DiezelVh4& n, float v)
             {
                 n.set_input_trim(v);
             },
             [](const DiezelVh4& n)
             {
                 return n.input_trim_db_;
             }},
            {{"output", "Output", K_MIN_OUTPUT_DB, K_MAX_OUTPUT_DB, 0.0f, ParamKind::Decibel, "dB"},
             [](DiezelVh4& n, float v)
             {
                 n.set_output(v);
             },
             [](const DiezelVh4& n)
             {
                 return n.output_db_;
             }},
        }}};
        return table;
    }

    float DiezelVh4::get_parameter(const char* id) const
    {
        return parameters().read(*this, id);
    }

    void DiezelVh4::set_parameter_for_control_thread(const char* id, float value)
    {
        parameters().write(*this, id, value);
    }

    void DiezelVh4::set_parameter_for_audio_thread(std::uint16_t index, float value)
    {
        parameters().write(*this, index, value);
    }

    namespace
    {
        std::unique_ptr<AudioNode> make_diezel_vh4(const PrepareInfo&)
        {
            return std::make_unique<DiezelVh4>();
        }
    } // namespace

    const NodeDescriptor& diezel_vh4_descriptor()
    {
        static const NodeDescriptor descriptor{
            "sushidsp.diezel_vh4",
            "VH4 Ch3",
            DiezelVh4::parameters().descriptors(),
            DiezelVh4::parameters().count(),
            PortLayout{1, 1},
            &make_diezel_vh4,
        };
        return descriptor;
    }
} // namespace SushiDSP
