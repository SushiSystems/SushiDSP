/**************************************************************************/
/* DiezelVh4.cpp                                                          */
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
 * @file DiezelVh4.cpp
 * @brief The VH4 channel-3 chain, its constants and its descriptor.
 * @author Mustafa Garip & Sushi Systems
 */

// _USE_MATH_DEFINES must precede every include: it is what makes MSVC's <cmath> declare M_PI.
#define _USE_MATH_DEFINES

#include "SushiDSP/diezel_vh4/DiezelVh4.hpp"

#include "SushiDSP/circuits/stages/PowerTubePresets.hpp"

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
        // Every constant below cites the section of
        // docs/reference/research/2026-09-06-diezel-vh4-design-dossier.md it comes from. A
        // constant the dossier marks "not established" or that this module chose for itself says
        // so on its own line and is listed again in modules/diezel_vh4/README.md.

        // Section 3.1: R1 1 MOhm grid leak with C1 printed as the range 100-200 pF across it.
        // C1 is not modelled: as a shunt across the input it sets its corner against the guitar's
        // source impedance, which this node never sees.

        // Section 3.1: V1a runs Ra 120 kOhm and Rk 2k2 fully bypassed by C2 47 uF, zero at 1.5 Hz.
        constexpr float kV1aGridLeakOhms = 1000000.0f;
        constexpr float kV1aCathodeBiasVolts = -2.05f;
        // Section 6: x61.4 unloaded, x56.8 into the 470 kOhm the channel-3 leg presents.
        constexpr float kV1aGain = 56.8f;
        // Modelling choice: section 3.1 draws no grid stopper on V1a, so the grid-conduction
        // source resistance is Jcm800.cpp's kTriodeSourceResistance.
        constexpr float kV1aSourceResistanceOhms = 10000.0f;
        /// @brief V1a's plate swing: 367 - 255.3 = 111.7 V to the supply, and 255.3 - 111.133 =
        /// 144.167 V to the bottom, the plate solved at Vgk = 0 on the 120 kOhm load line with the
        /// Koren 12AX7 (2.132 mA) above the 2.05 V cathode.
        constexpr PlateSwing kV1aPlateSwing{ 111.700f, 144.167f };

        // Sections 3.1 and 6: Ra 120 kOhm in parallel with rp 54.6 kOhm is the impedance C13 is
        // driven from, and it is what puts the corners below on the dossier's 314 Hz and 618 Hz.
        constexpr double kV1aPlateImpedanceOhms = 37530.0;
        constexpr double kGainCouplingCapacitorFarads = 1e-9;   // Section 3.1: C13.
        constexpr double kGainSeriesResistanceOhms = 220000.0;  // Section 3.1: R14.
        constexpr double kGainPotentiometerOhms = 250000.0;     // Section 3.2: the GAIN pot.

        // Section 3.3: R21 47 kOhm grid stopper, R22 3M3 grid leak as drawn - section 8 question
        // 5 records that a tech reports 1 MOhm and 220 kOhm on real boards - Vgk -1.02 V at 367 V.
        constexpr float kV2aSourceResistanceOhms = 47000.0f;
        constexpr float kV2aGridLeakOhms = 3300000.0f;
        constexpr float kV2aCathodeBiasVolts = -1.02f;
        constexpr float kV2aGain = 67.2f;   // Section 6: x67.2 into the 500 kOhm shunt.
        /// @brief V2a's plate swing: 367 - 186.2 = 180.8 V to the supply, and 186.2 - 110.343 =
        /// 75.857 V to the bottom, the plate solved at Vgk = 0 on the 120 kOhm load line with the
        /// Koren 12AX7 (2.139 mA) above the 1.02 V cathode.
        constexpr PlateSwing kV2aPlateSwing{ 180.800f, 75.857f };
        // Sections 3.3 and 6: Rk 680 Ohm with only C17 1 uF across it, 8.22 dB of loss below the
        // zero.
        constexpr double kV2aCathodeShelfZeroHz = 234.0;
        constexpr double kV2aCathodeShelfPoleHz = 603.0;

        // Section 6: C18 4.7 nF into V2a's 30.8 kOhm plate impedance plus the 500 kOhm shunt.
        constexpr double kV3aCouplingHighPassHz = 63.8;
        // Section 6: the relay node's 500 kOhm against 500 kOhm + 30.8 kOhm + R26 470 kOhm.
        constexpr float kV2aToV3aDivider = 0.4996003f;

        // Section 3.4: R26 470 kOhm grid stopper, the 500 kOhm R24'/R25' pair as the grid leak,
        // Vgk -1.02 V, Rk 680 Ohm with C19 2.2 uF.
        constexpr float kV3aSourceResistanceOhms = 470000.0f;
        constexpr float kV3aGridLeakOhms = 500000.0f;
        constexpr float kV3aCathodeBiasVolts = -1.02f;
        constexpr float kV3aGain = 71.4f;   // Section 6: x71.4.
        /// @brief V3a's plate swing, the same point V2a sits at: 367 - 186.2 = 180.8 V to the
        /// supply, and 186.2 - 110.343 = 75.857 V to the bottom, solved at Vgk = 0 on the 120 kOhm
        /// load line above the 1.02 V cathode.
        constexpr PlateSwing kV3aPlateSwing{ 180.800f, 75.857f };
        constexpr double kV3aCathodeShelfZeroHz = 106.0;
        constexpr double kV3aCathodeShelfPoleHz = 274.0;

        // Sections 3.5 and 6: the DC-coupled follower's gain is 0.986, and its output corner is
        // 2.26 Hz. That corner is C20's 220 nF into the 320 kOhm the follower actually looks into,
        // R30 1 MOhm in parallel with R31 470 kOhm; C20 into R30 alone would be 0.723 Hz. Section
        // 3.6 rules out the 10 uF/470 kOhm branch as a voicing element - "Do not implement this
        // network from S1" - but it also computes that branch as a plain 470 kOhm shunt across the
        // whole band, because 10 uF puts its corner at 0.034 Hz, so the load stays. The follower
        // itself is a scalar and this high-pass: its own compression is not modelled.
        constexpr float kCathodeFollowerGain = 0.986f;
        constexpr double kFollowerHighPassHz = 2.26;

        // Sections 2 and 4: the stack's output leaves through R42 470 kOhm into the 470 kOhm
        // channel volume, so the wiper's own fraction sits on a fixed 470/(470+470) divider.
        constexpr float kVolumeSeriesDivider = 0.5f;
        // Section 6: ch3 selected, R43 680 kOhm into the mix node's 48.1 kOhm, x0.0660, -23.6 dB.
        // Section 8 question 13: a single-channel module hard-codes the shunt the other three
        // grounded legs make.
        constexpr float kMixBusAttenuation = 0.0660f;

        // Sections 2 and 6: V4a runs Ra 120 kOhm with its 640 Ohm cathode unbypassed, which is
        // already folded into the x28.7 gain. Its grid is the mix node, so the source resistance
        // is R43 680 kOhm in parallel with the node's 48.1 kOhm and the grid leak is that node.
        constexpr float kV4aSourceResistanceOhms = 44923.0f;
        constexpr float kV4aGridLeakOhms = 48100.0f;
        constexpr float kV4aCathodeBiasVolts = -0.98f;
        constexpr float kV4aGain = 28.7f;
        /// @brief V4a's plate swing: 367 - 183.0 = 184.0 V to the supply, and 183.0 - 110.312 =
        /// 72.688 V to the bottom, the plate solved at Vgk = 0 on the 120 kOhm load line with the
        /// Koren 12AX7 (2.139 mA) above the 0.98 V cathode. The 640 Ohm cathode is unbypassed, so
        /// Vgk = 0 is read as the conduction limit at the idle cathode voltage; the README says so.
        constexpr PlateSwing kV4aPlateSwing{ 184.000f, 72.688f };
        // Modelling choice: section 2 draws V4b as the same follower topology as V3b on the same
        // 120 kOhm cathode, and the dossier gives a follower gain only for V3b, so V3b's 0.986 is
        // reused. The 100k/4.7k send pad (section 6, x0.0449) is on the jack leg and not in the
        // path to the power amp, so it is not applied here.

        // Section 5: nothing past the send jack is established. The loop return driver and the
        // phase inverter are one triode at Jcm800.cpp's kPhaseInverterGain of 40, on its triode
        // constants. A modelling choice, top to bottom.
        constexpr float kPhaseInverterSourceResistanceOhms = 10000.0f;
        constexpr float kPhaseInverterGridLeakOhms = 1000000.0f;
        constexpr float kPhaseInverterCathodeBiasVolts = -1.5f;
        constexpr float kPhaseInverterGain = 40.0f;
        /// @brief The phase inverter's plate swing, a modelling choice like the rest of this
        /// stage: the JCM800 long-tailed pair of docs/reference/spice/jcm_ltp/README.md at B+
        /// 360 V, whose 82 kOhm half self-biases to Vk 3.910 V, Ia 0.193 mA and Va 344.199 V
        /// against the Koren 12AX7, giving 360 - 344.199 = 15.801 V to the supply and 344.199 -
        /// 134.665 = 209.534 V to the bottom. jcm_ltp_tran.csv corroborates it: the differential
        /// probe reaches -20.74 V to +22.45 V at a 1 V drive, about twice one plate's cutoff side.
        constexpr PlateSwing kPhaseInverterPlateSwing{ 15.801f, 209.534f };
        /// @brief The tail degeneration a long-tailed pair applies to its own grid-to-cathode
        /// window, inherited with the stand-in and so a modelling choice like the rest of this
        /// stage. The cathode follows the grid, so the window is the input divided by
        /// 1 + (mu + 1)*R_tail/(r_a + Ra). At the JCM800 pair's operating point the Koren 12AX7
        /// gives mu 75.0428 and r_a 171.5 kOhm, and jcm_ltp's tail is 470 + 10k = 10.47 kOhm
        /// against the 82 kOhm plate: 1 + 76.0428 * 10470 / (171524 + 82000) = 4.140398.
        constexpr float kPhaseInverterDegeneration = 4.140398f;

        constexpr float kTriodeSaturationCurrentAmps = 1e-9f;
        constexpr float kTriodeThermalVoltageVolts = 0.02585f;

        // The four level-staging factors this module carried until 2026-09-07 are gone:
        // docs/superpowers/specs/2026-09-07-triode-plate-swing-design.md replaced them with the
        // plate-swing ceiling above, so the dossier's own dividers now run at face value in volts
        // from V1a's grid to the power tubes. The interstage factors left in the chain are all
        // dossier values: kV2aToV3aDivider, kVolumeSeriesDivider, kMixBusAttenuation and the two
        // cathode-follower scalars.
        /// @brief The bridge between the volt-true preamp and PentodeStage's grid units, until
        /// PentodeStage becomes volt-true (DA5). Its grid saturates at
        /// 1 / (plate_saturation_drive * gain) = 1 / (0.045 * 8) = 2.778 model volts, where
        /// kDiezelVh4PowerEl34's real grid needs |-36 V|: 2.778 / 36 = 0.077160.
        constexpr float kPowerTubeGridScale = 0.077160f;
        // Jcm800.cpp's constant, keeping its name because the name is the citation. It is this
        // chain's one output trim to full scale.
        constexpr float kPowerTubeToTransformerAttenuation = 0.05f;

        // Modelling choices: section 5 establishes no output transformer primary impedance, no
        // turns ratio and no supply behaviour, so the transformer and the sag stage carry
        // Jcm800.cpp's constants unchanged.
        constexpr float kOutputTransformerRolloffHz = 60.0f;
        constexpr float kOutputTransformerLeakageHz = 8000.0f;
        constexpr float kOutputTransformerSaturationDrive = 0.3f;
        constexpr float kSagAmount = 0.3f;
        constexpr float kSagRecoveryTimeSeconds = 0.05f;

        // Sections 5 and 6: Presence "frequency centers at about 4kHz" (S2). Its depth is a
        // modelling choice, Jcm800.cpp's kMaxPresenceDb, because no VH4 source gives one.
        constexpr float kPresenceHz = 4000.0f;
        constexpr float kMaxPresenceDb = 9.0f;
        // Sections 5 and 6: Deep is "centered at 80Hz" (S2). S8 says 90 Hz, and section 8
        // question 9 records the disagreement. The 10 dB of lift is a modelling choice, and so is
        // the first-order shelf: section 5 and section 8 question 8 say the real control is an LC
        // section in the feedback loop.
        constexpr float kDeepHz = 80.0f;
        constexpr float kMaxDeepBoostDb = 10.0f;

        constexpr double kFilterQ = 0.707;

        // A coupling capacitor is a first-order high-pass, and at 314 Hz the difference between
        // 6 and 12 dB per octave below the corner is most of this channel's low end. Biquad has no
        // first-order high-pass, so one is built from set_first_order_shelf() with its zero far
        // below its pole: the response is first-order through the audio band and settles at
        // kHighPassStopbandGain, -80 dB, at DC.
        constexpr double kHighPassStopbandGain = 1e-4;

        // 12AX7 Miller input capacitance, exactly as Jcm800.cpp computes it: Cgk 1.6 pF and
        // Cgp 1.7 pF from docs/reference/spice/models/12ax7_koren.sub, plus 10 pF of stray wiring.
        // Unlike the JCM800's, the driving resistance here is per grid, because sections 3.3 and
        // 3.4 draw the two grid stoppers this channel has and section 3.4 computes the V3a pole
        // from R26 itself.
        constexpr double kMillerCgkPicofarads = 1.6;
        constexpr double kMillerCgpPicofarads = 1.7;
        constexpr double kStrayWiringPicofarads = 10.0;

        constexpr float kSmoothingMs = 20.0f;
        constexpr float kCoefficientEpsilon = 1e-4f;

        constexpr float kMinMasterDb = -60.0f;
        constexpr float kMaxMasterDb = 12.0f;
        constexpr float kMinInputTrimDb = -24.0f;
        constexpr float kMaxInputTrimDb = 24.0f;
        constexpr float kMinOutputDb = -60.0f;
        constexpr float kMaxOutputDb = 12.0f;

        /// @brief The corner of one driven grid's Miller input-capacitance pole.
        /// @param stage_voltage_gain The driven stage's midband gain.
        /// @param driving_resistance_ohms The resistance the grid is driven through.
        /// @return The -3 dB corner in Hz.
        double miller_corner_hz(double stage_voltage_gain, double driving_resistance_ohms)
        {
            const double input_capacitance_farads =
                (kMillerCgkPicofarads + kMillerCgpPicofarads * (1.0 + stage_voltage_gain)
                 + kStrayWiringPicofarads) * 1e-12;
            return 1.0 / (2.0 * M_PI * driving_resistance_ohms * input_capacitance_farads);
        }

        /// @brief Configures one Biquad as a first-order high-pass.
        /// @param filter The filter to configure.
        /// @param sample_rate The rate it runs at, in hertz.
        /// @param corner_hz The -3 dB corner, in hertz.
        void configure_first_order_high_pass(Biquad& filter, double sample_rate, double corner_hz)
        {
            filter.set_first_order_shelf(sample_rate, corner_hz * kHighPassStopbandGain, corner_hz,
                                         kHighPassStopbandGain);
        }

        /// @brief The total resistance C13 works against for one gain-knob position.
        /// @param normalized The gain knob position in [0, 1].
        /// @return V1a's plate impedance plus R14 plus the pot's shunt leg, in ohms.
        double gain_network_resistance_ohms(double normalized)
        {
            return kV1aPlateImpedanceOhms + kGainSeriesResistanceOhms
                 + normalized * kGainPotentiometerOhms;
        }

        /// @brief The gain pot's own division ratio.
        /// @param normalized The gain knob position in [0, 1].
        /// @return The fraction of V1a's plate signal that reaches R21.
        float gain_attenuation(float normalized)
        {
            const double total = gain_network_resistance_ohms(normalized);
            return static_cast<float>(normalized * kGainPotentiometerOhms / total);
        }

        /// @brief Presence in decibels for a knob position.
        /// @param normalized Presence knob position in [0, 1]; 0.5 is flat.
        /// @return The shelf gain in decibels.
        float presence_gain_db(float normalized)
        {
            return (normalized - 0.5f) * 2.0f * kMaxPresenceDb;
        }
    }

    DiezelVh4::DiezelVh4(std::vector<float> cabinet_impulse_response)
        : cabinet_impulse_response_(std::move(cabinet_impulse_response))
        , v1a_(kV1aSourceResistanceOhms, kV1aGridLeakOhms, kV1aCathodeBiasVolts, kV1aGain, kTriodeSaturationCurrentAmps, kTriodeThermalVoltageVolts, kV1aPlateSwing)
        , v2a_(kV2aSourceResistanceOhms, kV2aGridLeakOhms, kV2aCathodeBiasVolts, kV2aGain, kTriodeSaturationCurrentAmps, kTriodeThermalVoltageVolts, kV2aPlateSwing)
        , v3a_(kV3aSourceResistanceOhms, kV3aGridLeakOhms, kV3aCathodeBiasVolts, kV3aGain, kTriodeSaturationCurrentAmps, kTriodeThermalVoltageVolts, kV3aPlateSwing)
        , tone_stack_(48000.0 * kOversamplingFactor)
        , v4a_(kV4aSourceResistanceOhms, kV4aGridLeakOhms, kV4aCathodeBiasVolts, kV4aGain, kTriodeSaturationCurrentAmps, kTriodeThermalVoltageVolts, kV4aPlateSwing)
        , phase_inverter_(kPhaseInverterSourceResistanceOhms, kPhaseInverterGridLeakOhms, kPhaseInverterCathodeBiasVolts, kPhaseInverterGain, kTriodeSaturationCurrentAmps, kTriodeThermalVoltageVolts, kPhaseInverterPlateSwing)
        , el34_a_(kDiezelVh4PowerEl34)
        , el34_b_(kDiezelVh4PowerEl34)
        , output_transformer_(48000.0 * kOversamplingFactor, kOutputTransformerRolloffHz, kOutputTransformerLeakageHz, kOutputTransformerSaturationDrive)
        , sag_(48000.0 * kOversamplingFactor, kSagAmount, kSagRecoveryTimeSeconds)
        , deep_shelf_(48000.0, kDeepHz, kMaxDeepBoostDb)
    {
        rebuild_gain_high_pass(gain_.target());
    }

    void DiezelVh4::prepare(const PrepareInfo& info)
    {
        sample_rate_ = info.sample_rate;
        const double oversampled_rate = sample_rate_ * kOversamplingFactor;

        presence_shelf_.set_high_shelf(sample_rate_, kPresenceHz, presence_gain_db(presence_.target()), kFilterQ);
        deep_shelf_ = ResonanceControl(sample_rate_, kDeepHz, kMaxDeepBoostDb);
        deep_shelf_.set_depth(deep_.target());

        // Everything from the gain pot's coupling capacitor to the sag stage sits inside the
        // single oversampled region, so every one of those corners is derived at 8x the host rate.
        rebuild_gain_high_pass(gain_.target());
        v2a_cathode_shelf_.set_first_order_shelf(oversampled_rate, kV2aCathodeShelfZeroHz, kV2aCathodeShelfPoleHz,
                                                 kV2aCathodeShelfZeroHz / kV2aCathodeShelfPoleHz);
        v2a_grid_miller_.set_one_pole_low_pass(oversampled_rate, miller_corner_hz(kV2aGain, kV2aSourceResistanceOhms));
        configure_first_order_high_pass(v3a_coupling_high_pass_, oversampled_rate, kV3aCouplingHighPassHz);
        v3a_cathode_shelf_.set_first_order_shelf(oversampled_rate, kV3aCathodeShelfZeroHz, kV3aCathodeShelfPoleHz,
                                                 kV3aCathodeShelfZeroHz / kV3aCathodeShelfPoleHz);
        v3a_grid_miller_.set_one_pole_low_pass(oversampled_rate, miller_corner_hz(kV3aGain, kV3aSourceResistanceOhms));
        configure_first_order_high_pass(follower_high_pass_, oversampled_rate, kFollowerHighPassHz);

        tone_stack_ = DiezelVh4ToneStack(oversampled_rate);
        output_transformer_ = OutputTransformerStage(oversampled_rate, kOutputTransformerRolloffHz, kOutputTransformerLeakageHz, kOutputTransformerSaturationDrive);
        sag_ = PowerSupplySagStage(oversampled_rate, kSagAmount, kSagRecoveryTimeSeconds);

        SmoothedParameter* const smoothed[] = {
            &gain_, &bass_, &middle_, &treble_, &volume_, &deep_, &presence_,
            &master_gain_, &input_trim_gain_, &output_gain_
        };
        for (SmoothedParameter* parameter : smoothed)
        {
            parameter->prepare(sample_rate_, kSmoothingMs);
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
        scratch_oversampled_.assign(static_cast<std::size_t>(info.max_block) * kOversamplingFactor, 0.0f);

        cabinet_partition_ = StreamingConvolution::partition_size_for(sample_rate_);
        if (!cabinet_impulse_response_.empty())
        {
            cabinet_slot_.publish(std::make_unique<StreamingConvolution>(
                cabinet_partition_, cabinet_impulse_response_.data(),
                static_cast<std::uint32_t>(cabinet_impulse_response_.size()), info.max_block));
        }

        reset();
    }

    void DiezelVh4::rebuild_gain_high_pass(float normalized)
    {
        const double corner_hz = 1.0 / (2.0 * M_PI * kGainCouplingCapacitorFarads
                                        * gain_network_resistance_ohms(normalized));
        configure_first_order_high_pass(gain_high_pass_, sample_rate_ * kOversamplingFactor, corner_hz);
    }

    void DiezelVh4::process(ProcessContext& context)
    {
        const float* input = context.inputs[0];
        float* output = context.outputs[0];
        const std::uint32_t frames = context.frames;
        const std::uint32_t oversampled_frames = frames * kOversamplingFactor;

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

        if (std::fabs(gain - applied_gain_) > kCoefficientEpsilon)
        {
            rebuild_gain_high_pass(gain);
            applied_gain_ = gain;
        }
        if (std::fabs(bass - applied_bass_) > kCoefficientEpsilon)
        {
            tone_stack_.set_bass(bass);
            applied_bass_ = bass;
        }
        if (std::fabs(middle - applied_middle_) > kCoefficientEpsilon)
        {
            tone_stack_.set_middle(middle);
            applied_middle_ = middle;
        }
        if (std::fabs(treble - applied_treble_) > kCoefficientEpsilon)
        {
            tone_stack_.set_treble(treble);
            applied_treble_ = treble;
        }
        if (std::fabs(deep - applied_deep_) > kCoefficientEpsilon)
        {
            deep_shelf_.set_depth(deep);
            applied_deep_ = deep;
        }
        if (std::fabs(presence - applied_presence_) > kCoefficientEpsilon)
        {
            presence_shelf_.set_high_shelf(sample_rate_, kPresenceHz, presence_gain_db(presence), kFilterQ);
            applied_presence_ = presence;
        }

        const float gain_pot = gain_attenuation(gain);
        const float volume_pot = volume * kVolumeSeriesDivider;
        const float master_gain = master * DiezelVh4::kMasterUnityGain;

        for (std::uint32_t i = 0; i < frames; ++i)
        {
            scratch_base_[i] = input[i] * input_gain;
        }

        const float* upsampled = oversampler_->upsample(scratch_base_.data(), frames);
        for (std::uint32_t i = 0; i < oversampled_frames; ++i)
        {
            const float v1a_out = v1a_.process_sample(upsampled[i]);
            const float v2a_in = v2a_grid_miller_.process_sample(
                v2a_cathode_shelf_.process_sample(gain_high_pass_.process_sample(v1a_out) * gain_pot));
            const float v2a_out = v3a_coupling_high_pass_.process_sample(v2a_.process_sample(v2a_in))
                                * kV2aToV3aDivider;
            const float v3a_in = v3a_grid_miller_.process_sample(v3a_cathode_shelf_.process_sample(v2a_out));
            const float follower_out = follower_high_pass_.process_sample(
                v3a_.process_sample(v3a_in) * kCathodeFollowerGain);
            const float mix_bus = tone_stack_.process_sample(follower_out)
                                * volume_pot * kMixBusAttenuation;
            const float send_buffer = v4a_.process_sample(mix_bus) * kCathodeFollowerGain;
            const float phase_inverted =
                phase_inverter_.process_sample(send_buffer * (1.0f / kPhaseInverterDegeneration))
                * kPowerTubeGridScale;
            const float el34_a_out = el34_a_.process_sample(phase_inverted);
            const float el34_b_out = el34_b_.process_sample(-phase_inverted);
            const float power_tube_out = (el34_a_out - el34_b_out) * kPowerTubeToTransformerAttenuation;

            const float transformer_out = output_transformer_.process_sample(power_tube_out);
            scratch_oversampled_[i] = sag_.process_sample(transformer_out);
        }
        oversampler_->downsample(scratch_oversampled_.data(), oversampled_frames, scratch_base_.data());

        // Deep and Presence are the two shelves inside the power amp's global feedback loop
        // (section 5),
        // so they sit after the output stage. Both are linear and generate nothing to alias, which
        // is why they run on this side of the decimator with the master volume.
        for (std::uint32_t i = 0; i < frames; ++i)
        {
            const float presence_out = presence_shelf_.process_sample(scratch_base_[i]);
            scratch_base_[i] = deep_shelf_.process_sample(presence_out) * master_gain;
        }

        StreamingConvolution* const cabinet = cabinet_slot_.acquire_for_audio_thread();
        if (cabinet != nullptr)
        {
            cabinet->process(scratch_base_.data(), output, frames);
        }
        else
        {
            for (std::uint32_t i = 0; i < frames; ++i)
            {
                output[i] = scratch_base_[i];
            }
        }

        // Nothing upstream is normalized to the DAC's range, so a hot gain/master/output
        // combination can still land outside [-1, 1]. A safety rail, not a tone stage.
        for (std::uint32_t i = 0; i < frames; ++i)
        {
            output[i] = std::clamp(output[i] * output_gain, -1.0f, 1.0f);
        }
    }

    std::uint32_t DiezelVh4::latency_samples() const
    {
        const std::uint32_t oversampling_latency = oversampler_ ? oversampler_->latency_samples() : 0;
        return oversampling_latency + (has_cabinet() ? cabinet_partition_ : 0);
    }

    const char* DiezelVh4::type_id() const { return "sushidsp.diezel_vh4"; }

    bool DiezelVh4::set_cabinet(Ir impulse_response)
    {
        if (impulse_response.samples.empty() || cabinet_partition_ == 0
            || impulse_response.sample_rate != sample_rate_)
        {
            return false;
        }
        cabinet_impulse_response_ = std::move(impulse_response.samples);
        cabinet_slot_.publish(std::make_unique<StreamingConvolution>(
            cabinet_partition_, cabinet_impulse_response_.data(),
            static_cast<std::uint32_t>(cabinet_impulse_response_.size()),
            static_cast<std::uint32_t>(scratch_base_.size())));
        return true;
    }

    void DiezelVh4::reclaim_cabinets()
    {
        cabinet_slot_.reclaim();
    }

    bool DiezelVh4::has_cabinet() const
    {
        return cabinet_slot_.has_active();
    }

    std::size_t DiezelVh4::triode_stage_count() const
    {
        return 5;
    }

    float DiezelVh4::grid_excursion_peak_volts(std::size_t stage) const
    {
        const TriodeStage* const stages[] = { &v1a_, &v2a_, &v3a_, &v4a_, &phase_inverter_ };
        if (stage >= triode_stage_count())
        {
            return 0.0f;
        }
        return stages[stage]->peak_grid_excursion_volts();
    }

    PortLayout DiezelVh4::ports() const
    {
        return { 1, 1 };
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
        if (StreamingConvolution* const cabinet = cabinet_slot_.active_for_publisher())
        {
            cabinet->reset();
        }
    }

    void DiezelVh4::set_gain(float normalized) { gain_.set_target(std::clamp(normalized, 0.0f, 1.0f)); }
    void DiezelVh4::set_bass(float normalized) { bass_.set_target(std::clamp(normalized, 0.0f, 1.0f)); }
    void DiezelVh4::set_middle(float normalized) { middle_.set_target(std::clamp(normalized, 0.0f, 1.0f)); }
    void DiezelVh4::set_treble(float normalized) { treble_.set_target(std::clamp(normalized, 0.0f, 1.0f)); }
    void DiezelVh4::set_volume(float normalized) { volume_.set_target(std::clamp(normalized, 0.0f, 1.0f)); }
    void DiezelVh4::set_deep(float normalized) { deep_.set_target(std::clamp(normalized, 0.0f, 1.0f)); }
    void DiezelVh4::set_presence(float normalized) { presence_.set_target(std::clamp(normalized, 0.0f, 1.0f)); }

    void DiezelVh4::set_master(float decibels)
    {
        master_db_ = std::clamp(decibels, kMinMasterDb, kMaxMasterDb);
        master_gain_.set_target(decibels_to_gain(master_db_));
    }

    void DiezelVh4::set_input_trim(float decibels)
    {
        input_trim_db_ = std::clamp(decibels, kMinInputTrimDb, kMaxInputTrimDb);
        input_trim_gain_.set_target(decibels_to_gain(input_trim_db_));
    }

    void DiezelVh4::set_output(float decibels)
    {
        output_db_ = std::clamp(decibels, kMinOutputDb, kMaxOutputDb);
        output_gain_.set_target(decibels_to_gain(output_db_));
    }

    bool DiezelVh4::tone_stack_is_dirty() const
    {
        return std::fabs(gain_.target() - applied_gain_) > kCoefficientEpsilon
            || std::fabs(bass_.target() - applied_bass_) > kCoefficientEpsilon
            || std::fabs(middle_.target() - applied_middle_) > kCoefficientEpsilon
            || std::fabs(treble_.target() - applied_treble_) > kCoefficientEpsilon
            || std::fabs(deep_.target() - applied_deep_) > kCoefficientEpsilon
            || std::fabs(presence_.target() - applied_presence_) > kCoefficientEpsilon;
    }

    float DiezelVh4::get_parameter(const char* id) const
    {
        if (std::strcmp(id, "gain") == 0) return gain_.target();
        if (std::strcmp(id, "bass") == 0) return bass_.target();
        if (std::strcmp(id, "middle") == 0) return middle_.target();
        if (std::strcmp(id, "treble") == 0) return treble_.target();
        if (std::strcmp(id, "volume") == 0) return volume_.target();
        if (std::strcmp(id, "deep") == 0) return deep_.target();
        if (std::strcmp(id, "presence") == 0) return presence_.target();
        if (std::strcmp(id, "master") == 0) return master_db_;
        if (std::strcmp(id, "input_trim") == 0) return input_trim_db_;
        if (std::strcmp(id, "output") == 0) return output_db_;
        return 0.0f;
    }

    void DiezelVh4::set_parameter(const char* id, float value)
    {
        if (std::strcmp(id, "gain") == 0) { set_gain(value); return; }
        if (std::strcmp(id, "bass") == 0) { set_bass(value); return; }
        if (std::strcmp(id, "middle") == 0) { set_middle(value); return; }
        if (std::strcmp(id, "treble") == 0) { set_treble(value); return; }
        if (std::strcmp(id, "volume") == 0) { set_volume(value); return; }
        if (std::strcmp(id, "deep") == 0) { set_deep(value); return; }
        if (std::strcmp(id, "presence") == 0) { set_presence(value); return; }
        if (std::strcmp(id, "master") == 0) { set_master(value); return; }
        if (std::strcmp(id, "input_trim") == 0) { set_input_trim(value); return; }
        if (std::strcmp(id, "output") == 0) { set_output(value); return; }
    }

    void DiezelVh4::set_parameter(std::uint16_t index, float value)
    {
        switch (index)
        {
        case 0: set_gain(value); break;
        case 1: set_bass(value); break;
        case 2: set_middle(value); break;
        case 3: set_treble(value); break;
        case 4: set_volume(value); break;
        case 5: set_deep(value); break;
        case 6: set_presence(value); break;
        case 7: set_master(value); break;
        case 8: set_input_trim(value); break;
        case 9: set_output(value); break;
        default: break;
        }
    }

    namespace
    {
        // The order is a contract: a host addresses these by index, and a preset written by one
        // build is read back by another, so entries are appended - never reordered or removed.
        constexpr ParameterDescriptor kDiezelVh4Parameters[] = {
            { "gain",       "Gain",       0.0f,             1.0f,             0.5f, ParamKind::Linear,  ""   },
            { "bass",       "Bass",       0.0f,             1.0f,             0.5f, ParamKind::Linear,  ""   },
            { "middle",     "Middle",     0.0f,             1.0f,             0.5f, ParamKind::Linear,  ""   },
            { "treble",     "Treble",     0.0f,             1.0f,             0.5f, ParamKind::Linear,  ""   },
            { "volume",     "Volume",     0.0f,             1.0f,             0.5f, ParamKind::Linear,  ""   },
            { "deep",       "Deep",       0.0f,             1.0f,             0.5f, ParamKind::Linear,  ""   },
            { "presence",   "Presence",   0.0f,             1.0f,             0.5f, ParamKind::Linear,  ""   },
            { "master",     "Master",     kMinMasterDb,     kMaxMasterDb,     0.0f, ParamKind::Decibel, "dB" },
            { "input_trim", "Input Trim", kMinInputTrimDb,  kMaxInputTrimDb,  0.0f, ParamKind::Decibel, "dB" },
            { "output",     "Output",     kMinOutputDb,     kMaxOutputDb,     0.0f, ParamKind::Decibel, "dB" },
        };

        std::unique_ptr<IAudioNode> make_diezel_vh4(const PrepareInfo&)
        {
            return std::make_unique<DiezelVh4>();
        }
    }

    const NodeDescriptor& diezel_vh4_descriptor()
    {
        static const NodeDescriptor descriptor{
            "sushidsp.diezel_vh4",
            "VH4 Ch3",
            kDiezelVh4Parameters,
            10,
            PortLayout{ 1, 1 },
            &make_diezel_vh4,
        };
        return descriptor;
    }
}
