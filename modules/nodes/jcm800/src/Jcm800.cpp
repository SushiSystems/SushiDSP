/****************************************************************/
/* Jcm800.cpp                                                   */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

#include "SushiDSP/jcm800/Jcm800.hpp"

#include "SushiDSP/circuits/AudioTaperPot.hpp"
#include "SushiDSP/circuits/OutputSafetyClamp.hpp"
#include "SushiDSP/circuits/stages/MillerInputCapacitance.hpp"
#include "SushiDSP/circuits/stages/PhaseInverterPresets.hpp"
#include "SushiDSP/circuits/stages/PowerTubePresets.hpp"

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
    constexpr PowerTubePreset kEl34{
        33831.9f, 220000.0f, -38.0f, 6.882361f, kPentodeEL34GridConduction, PlateSwing{ 25.5f, 280.2777f } };

        // Component values are physically-motivated approximations for a 12AX7/EL34 JCM800-style
        // signal path, not measured from a real amplifier (no reference unit or captures
        // available - see spec section 8's "Perceptual A/B validation... deferred").
        constexpr float kTriodeSourceResistance = 10000.0f;
        constexpr float kTriodeGridLeak = 1000000.0f;

        /// @brief V1a's gain, mu*Ra/(ra + Ra) from the Koren Jacobian at its idle point below:
        /// gm 1.5025 mA/V, ra 58.465 kOhm, mu 87.8412 on the 100 kOhm plate, the cathode bypassed.
        constexpr float kV1aGain = 55.432497f;

        /// @brief V1b's gain with its 10 kOhm cathode unbypassed, mu*Ra/(ra + Ra + (mu + 1)*Rk) at
        /// its idle point: gm 0.6812 mA/V, ra 115.768 kOhm, mu 78.8604; jcm_v1b_cold's AC sweep
        /// reads +16.89 dB (docs/archive/superpowers/specs/2026-09-07-grid-knee-antialiasing-design.md
        /// section 6).
        constexpr float kV1bGain = 7.774305f;

        /// @brief V2a's gain, mu*Ra/(ra + Ra) at its idle point below: gm 2.2682 mA/V, ra
        /// 41.923 kOhm, mu 95.0908 on the 100 kOhm plate, the cathode bypassed.
        constexpr float kV2aGain = 67.001549f;

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

        /// @brief The 2203's long-tailed pair, modelled as one triode on jcm_ltp's 82 kOhm plate
        /// over the 470 Ohm and 10 kOhm tail both cathodes share, with both grid leaks returned to
        /// the junction between them as the 2203 wires them; every figure is derived in
        /// include/SushiDSP/circuits/stages/PhaseInverterPresets.hpp.
        constexpr SingleTriodeInverter kPhaseInverter = kTriode12AX7Inverter82kPlate;

        /// @brief The class-AB bias mismatch between the two output tubes, an input-referred grid
        /// offset on the B side in real volts: the 0.03 model volts the chain carried before
        /// PentodeStage became volt-true, through the deleted 0.125 grid scale
        /// (modules/nodes/jcm800/README.md, "The EL34 pair"). The transformer's 60 Hz high-pass blocks
        /// the DC term it introduces. Held in a member so that
        /// set_power_tube_grid_bias_mismatch_volts() can zero it.
        constexpr float kPowerTubeGridBiasMismatchVolts = 0.24f;

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
        constexpr float kPlateLoadOhms = 100000.0f;

        constexpr float kOutputTransformerRolloffHz = 60.0f;
        constexpr float kOutputTransformerLeakageHz = 8000.0f;
        constexpr float kOutputTransformerSaturationDrive = 0.3f;

        constexpr float kSagAmount = 0.3f;
        constexpr float kSagRecoveryTimeSeconds = 0.05f;

        /// @brief The ceiling of the preamp pot's attenuation, reached with the knob full up.
        constexpr float kPreampPotMaxAttenuation = 1.0f / 16.0f;

        /// @brief The level the output transformer stage receives at the pair's full clip: the
        /// 2 x 20 V x 0.05 the chain delivered before PentodeStage became volt-true.
        constexpr float kTransformerFullClipUnits = 2.0f;

        /// @brief The output trim from the EL34 plates to the transformer's units, applied after
        /// the last grid, so it sets output level rather than any stage's drive: the pair's
        /// full swing, kEl34's two headrooms summed, lands at kTransformerFullClipUnits.
        constexpr float kPowerTubeToTransformerAttenuation = kTransformerFullClipUnits
            / (kEl34.plate_swing.headroom_to_supply_volts + kEl34.plate_swing.headroom_to_bottom_volts);

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

        /// @brief Returns the corner of one grid's Miller input-capacitance pole.
        /// @param stage_voltage_gain The driven stage's mid-band gain, which is what the
        /// grid-plate capacitance is multiplied by.
        /// @return The -3 dB corner of the grid's RC pole, in Hz.
        double miller_corner_hz(double stage_voltage_gain)
        {
            return kTriode12AX7MillerCapacitance.corner_hz(stage_voltage_gain, kPlateLoadOhms);
        }

        // The 2203's preamp volume is a pot between V1a's plate and V1b's grid, so the knob is an
        // attenuator, not an input multiplier: turning it up feeds V1b (and everything after it)
        // harder, which is what produces the cascaded-gain-stage distortion rather than just a
        // louder clean signal.
        //
        // The knob follows the corpus audio taper. Its exponent is 1.5 rather than a plain square
        // because the cascade's clean-to-saturated transition spans a wide span of interstage
        // level (a grid conducts once it reaches its cathode, and cuts off 4.14 V below), so a
        // steeper taper compresses too much of that transition into the top third of the knob's
        // travel.
        // Measured at a 0.2 input: with a square taper the THD proxy is zero below preamp 0.5,
        // with this one it starts moving at 0.4 and climbs monotonically to 1.0.
        float preamp_attenuation(float normalized)
        {
            return audio_taper_pot(normalized) * kPreampPotMaxAttenuation;
        }
    }

    Jcm800::Jcm800()
        : v1a_(kTriodeSourceResistance, kTriodeGridLeak, kV1aCathodeBias, kV1aGain, kTriode12AX7GridConduction, kV1aPlateSwing)
        , v1b_(kTriodeSourceResistance, kTriodeGridLeak, kV1bCathodeBias, kV1bGain, kTriode12AX7GridConduction, kV1bPlateSwing)
        , tone_stack_(48000.0 * kOversamplingFactor)
        , v2a_(kTriodeSourceResistance, kTriodeGridLeak, kV2aCathodeBias, kV2aGain, kTriode12AX7GridConduction, kV2aPlateSwing)
        , phase_inverter_(kTriodeSourceResistance, kTriodeGridLeak, kPhaseInverter.cathode_bias_volts, kPhaseInverter.gain, kTriode12AX7GridConduction, kPhaseInverter.plate_swing)
        , el34_a_(kEl34)
        , el34_b_(kEl34)
        , output_transformer_(48000.0 * kOversamplingFactor, kOutputTransformerRolloffHz, kOutputTransformerLeakageHz, kOutputTransformerSaturationDrive)
        , sag_(48000.0 * kOversamplingFactor, kSagAmount, kSagRecoveryTimeSeconds)
    {
        power_tube_grid_bias_mismatch_volts_ = kPowerTubeGridBiasMismatchVolts;

        const double initial_oversampled_rate_hz = 48000.0 * kOversamplingFactor;
        v1b_grid_miller_.set_one_pole_low_pass(initial_oversampled_rate_hz, miller_corner_hz(kV1bGain));
        v2a_grid_miller_.set_one_pole_low_pass(initial_oversampled_rate_hz, miller_corner_hz(kV2aGain));
        phase_inverter_grid_miller_.set_one_pole_low_pass(initial_oversampled_rate_hz, miller_corner_hz(kPhaseInverter.gain));
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
        // are derived at the oversampled rate, each from the gain of the stage whose grid it is,
        // through miller_corner_hz(), which reads the gain constants and so tracks them.
        //
        // The phase inverter's pole is a property of *this* model's inverter, which is a single
        // triode at kPhaseInverter.gain. A real 2203's inverter is a long-tailed pair, whose
        // driven grid sees very little Miller multiplication because that half runs as a
        // grounded-grid stage with no inverting plate-to-grid path. D6d should not inherit this
        // pole when it replaces the inverter with the LTP.
        const double oversampled_rate_hz = sample_rate_ * kOversamplingFactor;
        v1b_grid_miller_.set_one_pole_low_pass(oversampled_rate_hz, miller_corner_hz(kV1bGain));
        v2a_grid_miller_.set_one_pole_low_pass(oversampled_rate_hz, miller_corner_hz(kV2aGain));
        phase_inverter_grid_miller_.set_one_pole_low_pass(oversampled_rate_hz, miller_corner_hz(kPhaseInverter.gain));

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
            const float phase_split = v2a_.process_sample(toned) / kPhaseInverter.input_degeneration;
            const float phase_inverted = phase_inverter_.process_sample(
                phase_inverter_grid_miller_.process_sample(phase_split));
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

        // Cabinet convolution lives in the standalone sushidsp.cabinet node; this amp hands its
        // dry (post-presence, post-master) signal straight to output.
        for (std::uint32_t i = 0; i < frames; ++i)
        {
            output[i] = scratch_base_[i];
        }

        // Nothing upstream is normalized to the DAC's range - every stage is bounded only by its
        // own tube's conduction window - so a hot preamp/master/output combination can still land
        // outside [-1, 1]. The clamp is what makes that safe. It is deliberately not a soft-knee
        // limiter: a waveshaper here would need its own oversampled region to avoid aliasing its
        // own harmonics back down, and the amp's saturation character already comes from the tube
        // and transformer stages that model it. This is a safety rail, not a tone stage.
        apply_output_safety_clamp(output, frames, output_gain);
    }

    std::uint32_t Jcm800::latency_samples() const
    {
        return oversampler_ ? oversampler_->latency_samples() : 0;
    }

    const char* Jcm800::type_id() const { return "sushidsp.jcm800"; }

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

    const ParameterTable<Jcm800, 8>& Jcm800::parameters()
    {
        static const ParameterTable<Jcm800, 8> table{ {{
            { { "preamp", "Preamp", 0.0f, 1.0f, 0.5f, ParamKind::Linear, "" },
              [](Jcm800& n, float v) { n.set_preamp(v); },
              [](const Jcm800& n) { return n.preamp_.target(); } },
            { { "master", "Master", kMinMasterDb, kMaxMasterDb, 0.0f, ParamKind::Decibel, "dB" },
              [](Jcm800& n, float v) { n.set_master(v); },
              [](const Jcm800& n) { return n.master_db_; } },
            { { "bass", "Bass", 0.0f, 1.0f, 0.5f, ParamKind::Linear, "" },
              [](Jcm800& n, float v) { n.set_bass(v); },
              [](const Jcm800& n) { return n.bass_.target(); } },
            { { "middle", "Middle", 0.0f, 1.0f, 0.5f, ParamKind::Linear, "" },
              [](Jcm800& n, float v) { n.set_middle(v); },
              [](const Jcm800& n) { return n.middle_.target(); } },
            { { "treble", "Treble", 0.0f, 1.0f, 0.5f, ParamKind::Linear, "" },
              [](Jcm800& n, float v) { n.set_treble(v); },
              [](const Jcm800& n) { return n.treble_.target(); } },
            { { "presence", "Presence", 0.0f, 1.0f, 0.5f, ParamKind::Linear, "" },
              [](Jcm800& n, float v) { n.set_presence(v); },
              [](const Jcm800& n) { return n.presence_.target(); } },
            { { "input_trim", "Input Trim", kMinInputTrimDb, kMaxInputTrimDb, 0.0f,
                ParamKind::Decibel, "dB" },
              [](Jcm800& n, float v) { n.set_input_trim(v); },
              [](const Jcm800& n) { return n.input_trim_db_; } },
            { { "output", "Output", kMinOutputDb, kMaxOutputDb, 0.0f, ParamKind::Decibel, "dB" },
              [](Jcm800& n, float v) { n.set_output(v); },
              [](const Jcm800& n) { return n.output_db_; } },
        }} };
        return table;
    }

    float Jcm800::get_parameter(const char* id) const { return parameters().read(*this, id); }

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
            Jcm800::parameters().descriptors(),
            Jcm800::parameters().count(),
            PortLayout{ 1, 1 },
            &make_jcm800,
        };
        return descriptor;
    }
}
