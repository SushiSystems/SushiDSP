/**************************************************************************/
/* Jcm800.cpp                                                             */
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

// _USE_MATH_DEFINES must precede every include: it is what makes MSVC's <cmath> declare M_PI.
#define _USE_MATH_DEFINES

#include "SushiDSP/jcm800/Jcm800.hpp"

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
        // Component values are physically-motivated approximations for a 12AX7/EL34 JCM800-style
        // signal path, not measured from a real amplifier (no reference unit or captures
        // available - see spec section 8's "Perceptual A/B validation... deferred").
        constexpr float kTriodeSourceResistance = 10000.0f;
        constexpr float kTriodeGridLeak = 1000000.0f;
        constexpr float kTriodeGain = 60.0f;
        constexpr float kTriodeSaturationCurrent = 1e-9f;
        constexpr float kTriodeThermalVoltage = 0.02585f;

        /// @brief V1a's self-bias voltage: Rk 2.7 kOhm carrying the 0.855584 mA the Koren model
        /// settles at against a 100 kOhm plate load on a 360 V supply (jcm_v1a).
        constexpr float kV1aCathodeBias = -2.310076f;

        /// @brief V1a's plate swing: 360 - 274.441625 toward the supply, 274.441625 - 120.747068
        /// toward the plate bottom at Vgk = 0 on the same load line.
        constexpr PlateSwing kV1aPlateSwing{ 85.558375f, 153.694557f };

        /// @brief V1b's self-bias voltage: the cold clipper's unbypassed 10 kOhm cathode carrying
        /// 0.340132 mA against a 100 kOhm plate load on 360 V (jcm_v1b_cold).
        constexpr float kV1bCathodeBias = -3.401321f;

        /// @brief V1b's plate swing: 360 - 325.986790 toward the supply, 325.986790 - 121.553151
        /// toward the plate bottom at Vgk = 0, the cathode held at its idle voltage.
        constexpr PlateSwing kV1bPlateSwing{ 34.013210f, 204.433638f };

        /// @brief V2a's self-bias voltage: an 820 Ohm bypassed cathode carrying 1.519346 mA
        /// against a 100 kOhm plate load on 360 V (jcm_cf's V2a half).
        constexpr float kV2aCathodeBias = -1.245863f;

        /// @brief V2a's plate swing: 360 - 208.065440 toward the supply, 208.065440 - 119.960758
        /// toward the plate bottom at Vgk = 0 on the same load line.
        constexpr PlateSwing kV2aPlateSwing{ 151.934560f, 88.104683f };

        // The long-tailed pair, in one block so that a module whose inverter is a JCM800 stand-in
        // can copy the four constants and cite them. Every figure comes from jcm_ltp's 82 kOhm
        // plate, its shared 470 + 10 kOhm tail and the Koren Jacobian at the idle point below:
        // r_a 189506.723 Ohm, gm 391.832 uS, and so mu = gm*r_a = 74.254859.

        /// @brief The phase inverter's self-bias voltage: both halves share the 470 + 10 kOhm
        /// tail, so each sees 20.94 kOhm and settles at 0.187584 mA on 360 V (jcm_ltp).
        constexpr float kPhaseInverterCathodeBias = -3.928014f;

        /// @brief The inverter's plate swing on its 82 kOhm plate: 360 - 344.618092 toward the
        /// supply, 344.618092 - 134.677465 toward the bottom at Vgk = 0.
        constexpr PlateSwing kPhaseInverterPlateSwing{ 15.381908f, 209.940627f };

        /// @brief One triode's full load-line gain, mu*R_a/(r_a + R_a) =
        /// 74.254859*82000/271506.723. It is the whole single-triode gain, not half of it: the
        /// pair's split is already in the degeneration factor below, and halving it here would
        /// count that split twice.
        constexpr float kPhaseInverterGain = 22.426327f;

        /// @brief The tail degeneration, divided into the inverter's input because the shared
        /// cathode follows the driven grid. The other triode's cathode presents
        /// (r_a + R_a)/(mu + 1) = 271506.723/75.254859 = 3607.8298 Ohm in parallel with the tail,
        /// so the grid-to-cathode signal is the input over 1 + R_tail/(R_tail + that), which is
        /// 1 + 20940/(20940 + 3607.8298).
        constexpr float kPhaseInverterDegeneration = 1.853029f;

        /// @brief The bridge between the volt-true preamp and PentodeStage's grid units, until
        /// PentodeStage becomes volt-true (DA5). kEl34's grid saturates at 1/(0.05 * 8) = 2.5
        /// model volts where the preset's cathode_bias_volts field asks for |-20|, so the link
        /// carries 2.5/20. The 2203's own bias is nearer -35 V; the preset's field is what this
        /// scale has to match.
        constexpr float kPowerTubeGridScale = 0.125f;

        // The class-AB bias mismatch between the two output tubes, expressed where the model can
        // actually see it: as an input-referred grid offset on the B side, applied before the
        // stage.
        //
        // It cannot be expressed as a difference in the two stages' cathode-bias constants, which
        // is what this used to be. PentodeStage's node equation is bias-invariant by construction
        // (deliberately - it is what keeps a -20 V EL34 from clipping at silence): substituting
        // u = Vg - Vk cancels Vk out of the source term, the grid leak, the solver bounds, the
        // warm start and the plate output alike, so two instances differing only in Vk are the
        // same function of their input and the pair stays exactly odd-symmetric - no even
        // harmonics, no crossover character at all.
        //
        // An offset added to the input sits outside that Vk-relative coordinate system, so it
        // survives. It shifts B's conduction window relative to A's, which is what a real bias
        // mismatch does: the two tubes hand over at slightly different points in the cycle. The
        // magnitude is ~8% of the 0.38 V peak grid drive the phase inverter delivers here, in the
        // range of an ordinary unmatched pair, and comparable to the 0.203 V conduction knee, so
        // it moves the handover without turning the pair into a single-ended stage. The DC term
        // it introduces is blocked downstream by the output transformer's 60 Hz high-pass, as a
        // real transformer blocks it.
        // Held in a member rather than read here directly, so that
        // set_power_tube_grid_bias_mismatch_volts() can zero it and let a test measure what it
        // contributes; this value is the default the constructor installs.
        constexpr float kPowerTubeGridBiasMismatchVolts = 0.03f;

        // 12AX7 Miller input capacitance - the interstage bandwidth limit the model was missing.
        //
        // Every grid in the amp is driven through a real source impedance into a real input
        // capacitance, and for a high-mu triode that capacitance is dominated by the Miller
        // multiplication of the grid-plate capacitance: C_in = Cgk + Cgp * (1 + |A|). The two
        // capacitances are the 12AX7 model's own (docs/reference/spice/models/12ax7_koren.sub,
        // Cgk 1.6 pF and Cgp 1.7 pF), and |A| is the stage's mid-band voltage gain, so nothing
        // here is free-tuned; the only assumption is the stray wiring capacitance, taken as a
        // conventional 10 pF for point-to-point turret board wiring.
        //
        // The driving impedance is taken as a common 100 kOhm plate load for all three grids.
        // That is a deliberate simplification, not a per-node fact: V1b's grid is really fed from
        // the 1 MOhm preamp pot's wiper (a Thevenin source of up to 250 kOhm), V2a's from the
        // tone stack's output impedance, and the phase inverter's from V2a's plate through its
        // coupling cap. Every one of those is higher than 100 kOhm over most of its range, so the
        // errors all run in the same direction - this model is brighter than the amp,
        // never darker - and none of them is available as a single number until D6d gives each
        // block a real source impedance.
        //
        // V1b's grid sits behind the 2203's 470k/470p treble peaker, which this model carries as
        // kV1bTreblePeakerDivider and its shelf: the network's own loss and its return to unity
        // above the corner. What it does not carry is the peaker's interaction with the grid
        // capacitance below, where the 470 pF puts itself in series with the ~115 pF grid and the
        // source drives the two together. The corner below is computed from the plate load and
        // the grid capacitance alone, so the real circuit's sits higher than this one's.
        //
        // Without this, the D6a single 8x region let V1a/V1b's full 192 kHz-wide harmonic
        // spectrum reach V2a, the phase inverter and the EL34 pair, whose own clipping turned it
        // into content past the 8x Nyquist that folded back into the audio band. A real 2203 has
        // never had that bandwidth to give away.
        constexpr float kMillerCgkPicofarads = 1.6f;
        constexpr float kMillerCgpPicofarads = 1.7f;
        constexpr float kStrayWiringPicofarads = 10.0f;
        constexpr float kPlateLoadOhms = 100000.0f;

        constexpr float kOutputTransformerRolloffHz = 60.0f;
        constexpr float kOutputTransformerLeakageHz = 8000.0f;
        constexpr float kOutputTransformerSaturationDrive = 0.3f;

        constexpr float kSagAmount = 0.3f;
        constexpr float kSagRecoveryTimeSeconds = 0.05f;

        /// @brief The ceiling of the preamp pot's attenuation, reached with the knob full up.
        constexpr float kPreampPotMaxAttenuation = 1.0f / 16.0f;

        /// @brief The output trim from the EL34 plates to full scale, applied after the last
        /// grid, so it sets output level rather than any stage's drive.
        constexpr float kPowerTubeToTransformerAttenuation = 0.05f;

        /// @brief The mid-band loss of the 2203's V1b input network: the 470 kOhm treble-peaker
        /// resistor against V1b's 1 MOhm grid leak, 1e6/(470e3 + 1e6) (jcm_v1b_cold).
        constexpr float kV1bTreblePeakerDivider = 0.680272f;

        /// @brief The corner of the treble peaker's shelf, the geometric mean of the network's
        /// zero at 1/(2*pi*470k*470p) = 720.484 Hz and its pole at 1.47 times that, 1059.112 Hz.
        constexpr float kV1bTreblePeakerHz = 873.541f;

        /// @brief The peaker's shelf height, which returns the divider above to unity over the
        /// top end: -20*log10(0.680272).
        constexpr float kV1bTreblePeakerShelfDb = 3.34635f;

        /// @brief The flat insertion loss of the passive 2203 tone stack, measured on
        /// FmvToneStack(kJcm2203ToneStack) at bass = middle = treble = 0.5 and 1 kHz: 0.4030488,
        /// -7.8928 dB, against the -7.84 dB jcm_fmv's own sweep reads (docs/reference/spice/
        /// jcm_fmv). JcmToneStack is unity at flat and does not carry it.
        constexpr float kToneStackInsertionLoss = 0.4030488f;

        constexpr float kBrightCapHz = 60.0f;
        constexpr float kCouplingCapHz = 120.0f;
        constexpr float kPresenceHz = 3000.0f;
        constexpr float kMaxPresenceDb = 9.0f;
        constexpr double kFilterQ = 0.707;

        constexpr float kSmoothingMs = 20.0f;

        // Below this, a recomputed tone-stack/presence coefficient set would be numerically
        // indistinguishable from the one already in place, so the audio thread skips the rebuild.
        // It is a cost filter, not a quality one: 1e-4 of a knob's 0..1 travel is far finer than
        // the ~1e-3 step a 7-bit MIDI controller can even express.
        constexpr float kCoefficientEpsilon = 1e-4f;

        constexpr float kMinMasterDb = -60.0f;
        constexpr float kMaxMasterDb = 12.0f;
        constexpr float kMinInputTrimDb = -24.0f;
        constexpr float kMaxInputTrimDb = 24.0f;
        constexpr float kMinOutputDb = -60.0f;
        constexpr float kMaxOutputDb = 12.0f;

        float presence_gain_db(float normalized)
        {
            return (normalized - 0.5f) * 2.0f * kMaxPresenceDb;
        }

        /// @brief The corner of one grid's Miller input-capacitance pole.
        /// @param stage_voltage_gain The driven stage's mid-band gain, which is what the
        /// grid-plate capacitance is multiplied by.
        /// @return The -3 dB corner of the grid's RC pole, in Hz.
        double miller_corner_hz(double stage_voltage_gain)
        {
            const double input_capacitance_farads =
                (kMillerCgkPicofarads + kMillerCgpPicofarads * (1.0 + stage_voltage_gain)
                 + kStrayWiringPicofarads) * 1e-12;
            return 1.0 / (2.0 * M_PI * kPlateLoadOhms * input_capacitance_farads);
        }

        // The 2203's preamp volume is a pot between V1a's plate and V1b's grid, so the knob is an
        // attenuator, not an input multiplier: turning it up feeds V1b (and everything after it)
        // harder, which is what produces the cascaded-gain-stage distortion rather than just a
        // louder clean signal.
        //
        // The exponent approximates an audio-taper pot. It is 1.5 rather than a plain square
        // because the cascade's clean-to-saturated transition spans a wide span of interstage
        // level (a grid conducts once it reaches its cathode, and cuts off 4.14 V below), so a
        // steeper taper compresses too much of that transition into the top third of the knob's
        // travel.
        // Measured at a 0.2 input: with a square taper the THD proxy is zero below preamp 0.5,
        // with this one it starts moving at 0.4 and climbs monotonically to 1.0.
        float preamp_attenuation(float normalized)
        {
            return normalized * std::sqrt(normalized) * kPreampPotMaxAttenuation;
        }
    }

    Jcm800::Jcm800(std::vector<float> cabinet_impulse_response)
        : cabinet_impulse_response_(std::move(cabinet_impulse_response))
        , v1a_(kTriodeSourceResistance, kTriodeGridLeak, kV1aCathodeBias, kTriodeGain, kTriodeSaturationCurrent, kTriodeThermalVoltage, kV1aPlateSwing)
        , v1b_(kTriodeSourceResistance, kTriodeGridLeak, kV1bCathodeBias, kTriodeGain, kTriodeSaturationCurrent, kTriodeThermalVoltage, kV1bPlateSwing)
        , tone_stack_(48000.0 * kOversamplingFactor)
        , v2a_(kTriodeSourceResistance, kTriodeGridLeak, kV2aCathodeBias, kTriodeGain, kTriodeSaturationCurrent, kTriodeThermalVoltage, kV2aPlateSwing)
        , phase_inverter_(kTriodeSourceResistance, kTriodeGridLeak, kPhaseInverterCathodeBias, kPhaseInverterGain, kTriodeSaturationCurrent, kTriodeThermalVoltage, kPhaseInverterPlateSwing)
        , el34_a_(kEl34)
        , el34_b_(kEl34)
        , output_transformer_(48000.0 * kOversamplingFactor, kOutputTransformerRolloffHz, kOutputTransformerLeakageHz, kOutputTransformerSaturationDrive)
        , sag_(48000.0 * kOversamplingFactor, kSagAmount, kSagRecoveryTimeSeconds)
    {
        power_tube_grid_bias_mismatch_volts_ = kPowerTubeGridBiasMismatchVolts;

        const double initial_oversampled_rate_hz = 48000.0 * kOversamplingFactor;
        v1b_grid_miller_.set_one_pole_low_pass(initial_oversampled_rate_hz, miller_corner_hz(kTriodeGain));
        v2a_grid_miller_.set_one_pole_low_pass(initial_oversampled_rate_hz, miller_corner_hz(kTriodeGain));
        phase_inverter_grid_miller_.set_one_pole_low_pass(initial_oversampled_rate_hz, miller_corner_hz(kPhaseInverterGain));
    }

    void Jcm800::prepare(const PrepareInfo& info)
    {
        sample_rate_ = info.sample_rate;
        bright_cap_.set_high_pass(sample_rate_, kBrightCapHz, kFilterQ);
        // The V1a->V1b coupling capacitor sits inside the oversampled region, so its coefficients
        // must be derived at the oversampled rate; deriving them at the base rate would place the
        // real corner 8x higher (960 Hz instead of 120 Hz) and gut the low end.
        //
        // It is a first-order high-pass because that is what a coupling capacitor into a grid
        // leak is; a Butterworth biquad would roll the low end off at twice the rate the RC does.
        // The first-order pole is also what keeps this stable at the oversampled rate: 120 Hz
        // against 8 x 192 kHz is a ratio of 7.8e-5, under set_high_pass's 1e-4 floor.
        coupling_cap_.set_first_order_high_pass(sample_rate_ * kOversamplingFactor, kCouplingCapHz);
        // The peaker sits inside the oversampled region with the coupling capacitor it follows.
        v1b_treble_peaker_.set_high_shelf(sample_rate_ * kOversamplingFactor, kV1bTreblePeakerHz,
                                          kV1bTreblePeakerShelfDb, kFilterQ);
        presence_shelf_.set_high_shelf(sample_rate_, kPresenceHz, presence_gain_db(presence_.target()), kFilterQ);

        // The Miller poles are part of the oversampled chain, so like the coupling capacitor they
        // are derived at the oversampled rate. Their corners land near 13.8 kHz for the two
        // gain-of-60 triode grids and near 19.6 kHz for the lower-gain phase inverter.
        //
        // The phase inverter's pole is a property of *this* model's inverter, which is a single
        // gain-of-40 triode. A real 2203's inverter is a long-tailed pair, whose driven grid sees
        // very little Miller multiplication because that half runs as a grounded-grid stage with
        // no inverting plate-to-grid path. D6d should not inherit this pole when it replaces the
        // inverter with the LTP.
        const double oversampled_rate_hz = sample_rate_ * kOversamplingFactor;
        v1b_grid_miller_.set_one_pole_low_pass(oversampled_rate_hz, miller_corner_hz(kTriodeGain));
        v2a_grid_miller_.set_one_pole_low_pass(oversampled_rate_hz, miller_corner_hz(kTriodeGain));
        phase_inverter_grid_miller_.set_one_pole_low_pass(oversampled_rate_hz, miller_corner_hz(kPhaseInverterGain));

        // The tone stack sits inside the single oversampled region (spec D6a.3: one region per
        // node), so its coefficients are derived at the oversampled rate. It is passive and
        // linear today and gains nothing from the higher rate; it is inside because collapsing
        // the two regions into one halves the node's latency, and because D6d turns it into a
        // DK-method block whose solver does belong at 8x.
        tone_stack_ = JcmToneStack(sample_rate_ * kOversamplingFactor);

        // Every knob is smoothed at the base rate, since every one of them is read once per block
        // in process() via skip(frames). prepare() is a discontinuity anyway (buffers resized,
        // filters rebuilt), so the smoothers snap rather than ramp from a stale value, and the
        // applied_* markers are invalidated so the first block builds the coefficients that match
        // those snapped targets.
        SmoothedParameter* const smoothed[] = {
            &preamp_, &presence_, &bass_, &middle_, &treble_,
            &master_gain_, &input_trim_gain_, &output_gain_
        };
        for (SmoothedParameter* parameter : smoothed)
        {
            parameter->prepare(sample_rate_, kSmoothingMs);
            parameter->snap(parameter->target());
        }
        applied_bass_ = -1.0f;
        applied_middle_ = -1.0f;
        applied_treble_ = -1.0f;
        applied_presence_ = -1.0f;

        // The output transformer and the sag stage run inside the oversampled region (see
        // process()), so both are built at the oversampled rate: their filter corners and the
        // sag recovery time constant are all derived from the rate they are stepped at.
        const double oversampled_rate = sample_rate_ * kOversamplingFactor;
        output_transformer_ = OutputTransformerStage(oversampled_rate, kOutputTransformerRolloffHz, kOutputTransformerLeakageHz, kOutputTransformerSaturationDrive);
        sag_ = PowerSupplySagStage(oversampled_rate, kSagAmount, kSagRecoveryTimeSeconds);

        oversampler_ = std::make_unique<Oversampler8x>(info.max_block);

        scratch_base_.assign(info.max_block, 0.0f);
        scratch_oversampled_.assign(static_cast<std::size_t>(info.max_block) * kOversamplingFactor, 0.0f);

        // StreamingConvolution re-blocks the stream internally, so the partition is a property of
        // the sample rate alone rather than of the host's block size: every block size, power of
        // two or not, gets the same cabinet and the same one-partition latency.
        cabinet_partition_ = StreamingConvolution::partition_size_for(sample_rate_);
        if (!cabinet_impulse_response_.empty())
        {
            cabinet_slot_.publish(std::make_unique<StreamingConvolution>(
                cabinet_partition_, cabinet_impulse_response_.data(),
                static_cast<std::uint32_t>(cabinet_impulse_response_.size()), info.max_block));
        }

        reset();
    }

    void Jcm800::process(ProcessContext& context)
    {
        const float* input = context.inputs[0];
        float* output = context.outputs[0];
        const std::uint32_t frames = context.frames;
        const std::uint32_t oversampled_frames = frames * kOversamplingFactor;

        // Control changes are only ever targets; the audio thread is where they take effect. The
        // per-block advance is a single closed-form step, and the coefficient rebuilds below are
        // plain arithmetic on stack values - no allocation, no lock, no syscall.
        const float preamp = preamp_.skip(frames);
        const float bass = bass_.skip(frames);
        const float middle = middle_.skip(frames);
        const float treble = treble_.skip(frames);
        const float presence = presence_.skip(frames);
        const float input_gain = input_trim_gain_.skip(frames);
        const float master = master_gain_.skip(frames);
        const float output_gain = output_gain_.skip(frames);

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
        if (std::fabs(presence - applied_presence_) > kCoefficientEpsilon)
        {
            presence_shelf_.set_high_shelf(sample_rate_, kPresenceHz, presence_gain_db(presence), kFilterQ);
            applied_presence_ = presence;
        }

        const float preamp_gain = preamp_attenuation(preamp);
        const float master_gain = master * Jcm800::kMasterUnityGain;

        for (std::uint32_t i = 0; i < frames; ++i)
        {
            scratch_base_[i] = bright_cap_.process_sample(input[i] * input_gain);
        }

        const float* upsampled = oversampler_->upsample(scratch_base_.data(), frames);
        for (std::uint32_t i = 0; i < oversampled_frames; ++i)
        {
            const float v1a_out = v1a_.process_sample(upsampled[i]);
            const float v1b_in = v1b_grid_miller_.process_sample(v1b_treble_peaker_.process_sample(
                coupling_cap_.process_sample(v1a_out) * preamp_gain * kV1bTreblePeakerDivider));
            const float v1b_out = v1b_.process_sample(v1b_in);
            const float toned = v2a_grid_miller_.process_sample(
                tone_stack_.process_sample(v1b_out) * kToneStackInsertionLoss);
            const float phase_split = v2a_.process_sample(toned) / kPhaseInverterDegeneration;
            const float phase_inverted = phase_inverter_.process_sample(
                phase_inverter_grid_miller_.process_sample(phase_split)) * kPowerTubeGridScale;
            const float el34_a_out = el34_a_.process_sample(phase_inverted);
            const float el34_b_out = el34_b_.process_sample(-phase_inverted - power_tube_grid_bias_mismatch_volts_);
            const float power_tube_out = (el34_a_out - el34_b_out) * kPowerTubeToTransformerAttenuation;

            // The transformer and the sag stage stay inside the oversampled region, as spec
            // section 5's diagram places them: the transformer's core saturation is a genuine
            // tanh nonlinearity and the sag stage's gain is signal-dependent, so running either
            // after the decimator would fold their generated harmonics back below Nyquist.
            const float transformer_out = output_transformer_.process_sample(power_tube_out);
            scratch_oversampled_[i] = sag_.process_sample(transformer_out);
        }
        oversampler_->downsample(scratch_oversampled_.data(), oversampled_frames, scratch_base_.data());

        // The presence shelf and the master volume are linear, so they generate nothing to alias
        // and cost 8x less on this side of the decimator.
        for (std::uint32_t i = 0; i < frames; ++i)
        {
            scratch_base_[i] = presence_shelf_.process_sample(scratch_base_[i]) * master_gain;
        }

        // One atomic acquire is the audio thread's entire share of a cabinet swap; the convolver
        // it hands back was built in full on the control thread.
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

        // Nothing upstream is normalized to the DAC's range - every stage is bounded only by its
        // own tube's conduction window - so a hot preamp/master/output combination can still land
        // outside [-1, 1]. The clamp is what makes that safe. It is deliberately not a soft-knee
        // limiter: a waveshaper here would need its own oversampled region to avoid aliasing its
        // own harmonics back down, and the amp's saturation character already comes from the tube
        // and transformer stages that model it. This is a safety rail, not a tone stage.
        for (std::uint32_t i = 0; i < frames; ++i)
        {
            output[i] = std::clamp(output[i] * output_gain, -1.0f, 1.0f);
        }
    }

    std::uint32_t Jcm800::latency_samples() const
    {
        const std::uint32_t oversampling_latency = oversampler_ ? oversampler_->latency_samples() : 0;
        return oversampling_latency + (has_cabinet() ? cabinet_partition_ : 0);
    }

    const char* Jcm800::type_id() const { return "sushidsp.jcm800"; }

    bool Jcm800::set_cabinet(Ir impulse_response)
    {
        // cabinet_partition_ is 0 until prepare() has run, and a zero-partition
        // StreamingConvolution has empty FIFOs that process() would index out of bounds. The rate
        // check alone does not catch this: sample_rate_ defaults to 48000, so a 48 kHz IR handed
        // to an unprepared node would otherwise sail past it. Rejected outright rather than
        // stashed - a false return means nothing was installed and nothing was armed.
        if (impulse_response.samples.empty() || cabinet_partition_ == 0
            || impulse_response.sample_rate != sample_rate_)
        {
            return false;
        }
        // Kept so a later prepare() can rebuild this cabinet at the new partition and block size.
        // RtSwapSlot has no "unpublish", so a cabinet that outlived its prepare() would otherwise
        // stay active with a stale partition and a latency figure that no longer described it.
        cabinet_impulse_response_ = std::move(impulse_response.samples);
        cabinet_slot_.publish(std::make_unique<StreamingConvolution>(
            cabinet_partition_, cabinet_impulse_response_.data(),
            static_cast<std::uint32_t>(cabinet_impulse_response_.size()),
            static_cast<std::uint32_t>(scratch_base_.size())));
        return true;
    }

    void Jcm800::reclaim_cabinets()
    {
        cabinet_slot_.reclaim();
    }

    bool Jcm800::has_cabinet() const
    {
        return cabinet_slot_.has_active();
    }

    PortLayout Jcm800::ports() const
    {
        return { 1, 1 };
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
        if (StreamingConvolution* const cabinet = cabinet_slot_.active_for_publisher())
        {
            cabinet->reset();
        }
    }

    void Jcm800::set_preamp(float normalized) { preamp_.set_target(std::clamp(normalized, 0.0f, 1.0f)); }
    void Jcm800::set_bass(float normalized) { bass_.set_target(std::clamp(normalized, 0.0f, 1.0f)); }
    void Jcm800::set_middle(float normalized) { middle_.set_target(std::clamp(normalized, 0.0f, 1.0f)); }
    void Jcm800::set_treble(float normalized) { treble_.set_target(std::clamp(normalized, 0.0f, 1.0f)); }
    void Jcm800::set_presence(float normalized) { presence_.set_target(std::clamp(normalized, 0.0f, 1.0f)); }

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
        const TriodeStage* const stages[] = { &v1a_, &v1b_, &v2a_, &phase_inverter_ };
        if (stage >= triode_stage_count())
        {
            return 0.0f;
        }
        return stages[stage]->peak_grid_excursion_volts();
    }

    void Jcm800::set_master(float decibels)
    {
        master_db_ = std::clamp(decibels, kMinMasterDb, kMaxMasterDb);
        master_gain_.set_target(decibels_to_gain(master_db_));
    }

    void Jcm800::set_input_trim(float decibels)
    {
        input_trim_db_ = std::clamp(decibels, kMinInputTrimDb, kMaxInputTrimDb);
        input_trim_gain_.set_target(decibels_to_gain(input_trim_db_));
    }

    void Jcm800::set_output(float decibels)
    {
        output_db_ = std::clamp(decibels, kMinOutputDb, kMaxOutputDb);
        output_gain_.set_target(decibels_to_gain(output_db_));
    }

    bool Jcm800::tone_stack_is_dirty() const
    {
        return std::fabs(bass_.target() - applied_bass_) > kCoefficientEpsilon
            || std::fabs(middle_.target() - applied_middle_) > kCoefficientEpsilon
            || std::fabs(treble_.target() - applied_treble_) > kCoefficientEpsilon
            || std::fabs(presence_.target() - applied_presence_) > kCoefficientEpsilon;
    }

    float Jcm800::get_parameter(const char* id) const
    {
        if (std::strcmp(id, "preamp") == 0) return preamp_.target();
        if (std::strcmp(id, "master") == 0) return master_db_;
        if (std::strcmp(id, "bass") == 0) return bass_.target();
        if (std::strcmp(id, "middle") == 0) return middle_.target();
        if (std::strcmp(id, "treble") == 0) return treble_.target();
        if (std::strcmp(id, "presence") == 0) return presence_.target();
        if (std::strcmp(id, "input_trim") == 0) return input_trim_db_;
        if (std::strcmp(id, "output") == 0) return output_db_;
        return 0.0f;
    }

    void Jcm800::set_parameter(const char* id, float value)
    {
        if (std::strcmp(id, "preamp") == 0) { set_preamp(value); return; }
        if (std::strcmp(id, "master") == 0) { set_master(value); return; }
        if (std::strcmp(id, "bass") == 0) { set_bass(value); return; }
        if (std::strcmp(id, "middle") == 0) { set_middle(value); return; }
        if (std::strcmp(id, "treble") == 0) { set_treble(value); return; }
        if (std::strcmp(id, "presence") == 0) { set_presence(value); return; }
        if (std::strcmp(id, "input_trim") == 0) { set_input_trim(value); return; }
        if (std::strcmp(id, "output") == 0) { set_output(value); return; }
    }

    void Jcm800::set_parameter(std::uint16_t index, float value)
    {
        switch (index)
        {
        case 0: set_preamp(value); break;
        case 1: set_master(value); break;
        case 2: set_bass(value); break;
        case 3: set_middle(value); break;
        case 4: set_treble(value); break;
        case 5: set_presence(value); break;
        case 6: set_input_trim(value); break;
        case 7: set_output(value); break;
        default: break;
        }
    }

    namespace
    {
        // The order is a contract: a host addresses these by index, and a preset written by one
        // build is read back by another, so entries are appended - never reordered or removed.
        constexpr ParameterDescriptor kJcm800Parameters[] = {
            { "preamp",     "Preamp",     0.0f,             1.0f,             0.5f, ParamKind::Linear,  ""   },
            { "master",     "Master",     kMinMasterDb,     kMaxMasterDb,     0.0f, ParamKind::Decibel, "dB" },
            { "bass",       "Bass",       0.0f,             1.0f,             0.5f, ParamKind::Linear,  ""   },
            { "middle",     "Middle",     0.0f,             1.0f,             0.5f, ParamKind::Linear,  ""   },
            { "treble",     "Treble",     0.0f,             1.0f,             0.5f, ParamKind::Linear,  ""   },
            { "presence",   "Presence",   0.0f,             1.0f,             0.5f, ParamKind::Linear,  ""   },
            { "input_trim", "Input Trim", kMinInputTrimDb,  kMaxInputTrimDb,  0.0f, ParamKind::Decibel, "dB" },
            { "output",     "Output",     kMinOutputDb,     kMaxOutputDb,     0.0f, ParamKind::Decibel, "dB" },
        };

        std::unique_ptr<IAudioNode> make_jcm800(const PrepareInfo&)
        {
            return std::make_unique<Jcm800>();
        }
    }

    const NodeDescriptor& jcm800_descriptor()
    {
        static const NodeDescriptor descriptor{
            "sushidsp.jcm800",
            "JCM800",
            kJcm800Parameters,
            8,
            PortLayout{ 1, 1 },
            &make_jcm800,
        };
        return descriptor;
    }
}
