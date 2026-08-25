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

#include "SushiDSP/jcm800/Jcm800.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>

namespace SushiDSP
{
    namespace
    {
        // Component values are physically-motivated approximations for a 12AX7/EL34 JCM800-style
        // signal path, not measured from a real amplifier (no reference unit or captures
        // available - see spec section 8's "Perceptual A/B validation... deferred").
        constexpr float kTriodeSourceResistance = 10000.0f;
        constexpr float kTriodeGridLeak = 1000000.0f;
        constexpr float kTriodeCathodeBias = -1.5f;
        constexpr float kTriodeGain = 60.0f;
        constexpr float kTriodeSaturationCurrent = 1e-9f;
        constexpr float kTriodeThermalVoltage = 0.02585f;

        constexpr float kPhaseInverterGain = 40.0f;

        constexpr float kPentodeSourceResistance = 10000.0f;
        constexpr float kPentodeGridLeak = 220000.0f;
        constexpr float kPentodeCathodeBias = -20.0f;
        constexpr float kPentodeGain = 8.0f;
        constexpr float kPentodeSaturationCurrent = 1e-9f;
        constexpr float kPentodeThermalVoltage = 0.02585f;
        constexpr float kPentodePlateSaturationDrive = 0.05f;

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
        constexpr float kPowerTubeGridBiasMismatchVolts = 0.03f;

        constexpr float kOutputTransformerRolloffHz = 60.0f;
        constexpr float kOutputTransformerLeakageHz = 8000.0f;
        constexpr float kOutputTransformerSaturationDrive = 0.3f;

        constexpr float kSagAmount = 0.3f;
        constexpr float kSagRecoveryTimeSeconds = 0.05f;

        // Interstage gain staging - the plate-load/grid-leak dividers and coupling networks that
        // sit at every interstage link in a real amp. Without them the four cascaded stages
        // multiply to a gain of 60*60*60*40 ~ 8.6e6 and every stage past V1a pins against its
        // solver bound, which makes the amp a fixed-amplitude square-wave generator with a dead
        // gain knob.
        //
        // What sets these values is the *operating level* each grid ends up at, not the ratio
        // itself. A 12AX7 grid conducts from about 0.203 V above its bias point (Vt*ln(Vt/(Is*Rs))
        // against the 10k source resistance) and its plate reaches cutoff around 4.14 V below it,
        // so a stage distorts only while its grid is driven inside roughly that 26 dB window - and
        // each stage inverts, so consecutive stages clip on opposite half-cycles. Because a stage's own gain
        // is 60, a divider near 1/60 is what carries that operating level forward unchanged; the
        // slightly looser 1/50 links add about 1.2x per stage to make up for the amplitude the
        // preceding stage's grid conduction removed, so the drive does not decay down the cascade.
        //
        // The preamp pot's ceiling is what puts the cascade into that window at all: at 1/16 it
        // lifts V1b's grid from V1a's plate swing to about 0.7 V at a 0.2 input with the knob full
        // up, i.e. well past conduction, where the earlier 1/60 left it at 0.19 V and the whole
        // chain measured as a single clipping input tube (see the Task 8 report's THD sweep).
        // Worst-case measured grid excursion across a preamp x input-amplitude sweep is V1b's
        // 1.56 V at a 0.5 input (38% of the 4.14 V cutoff bound) and 2.81 V at a 1.0 input (68%);
        // every other stage stays under 0.47 V because it is the conducting half that faces it.
        // See the Task 9 report's ruling, and the bound's own comment in TriodeStage.cpp.
        //
        // kPowerTubeToTransformerAttenuation is different: it sits after the EL34 plates, so it is
        // an output-level trim into the transformer, not a grid-bound constant. It was halved
        // alongside the drive increase so the added preamp saturation shows up as harmonic content
        // rather than as 6 dB of extra output, keeping the master knob's useful range and the
        // output transformer's own saturation point where they were.
        constexpr float kPreampPotMaxAttenuation = 1.0f / 16.0f;
        constexpr float kV1bToV2aAttenuation = 1.0f / 50.0f;
        constexpr float kV2aToPhaseInverterAttenuation = 1.0f / 50.0f;
        constexpr float kPhaseInverterToPowerTubeAttenuation = 1.0f / 32.0f;
        constexpr float kPowerTubeToTransformerAttenuation = 0.05f;

        constexpr float kBrightCapHz = 60.0f;
        constexpr float kCouplingCapHz = 120.0f;
        constexpr float kPresenceHz = 3000.0f;
        constexpr float kMaxPresenceDb = 9.0f;
        constexpr double kFilterQ = 0.707;

        constexpr float kSlewPerBlock = 0.1f;

        float slew_toward(float current, float target)
        {
            return current + (target - current) * kSlewPerBlock;
        }

        // Every stage up to here saturates against its own solver bound, so a musically hot but
        // otherwise legitimate drive/preamp/master combination can still land the final sample
        // outside [-1, 1] - nothing upstream of this point is normalized to the DAC's range, only
        // to each tube's own conduction window. Left alone, that overshoot is hard-clipped by
        // whatever sits downstream (the driver, the DAC), which sounds nothing like the amp - it's
        // a flat-topped digital ceiling, not the transformer/speaker compression a real loud JCM800
        // exhibits at its output. kOutputLimiterThreshold is chosen well above the entire
        // regression corpus (largest sample ~0.157, see test_jcm800_regression.cpp) specifically so
        // this is the identity function - and therefore silent - at every level anyone has actually
        // measured; it only starts doing anything within the last ~1 dB of full scale.
        constexpr float kOutputLimiterThreshold = 0.9f;

        // Smooth (C1-continuous at the threshold - tanh'(0) == 1, matching the identity region's
        // slope) soft-knee limiter: identity below threshold, asymptoting toward +-1 above it.
        float soft_limit(float sample, float threshold)
        {
            const float magnitude = std::fabs(sample);
            if (magnitude <= threshold)
            {
                return sample;
            }
            const float sign = sample < 0.0f ? -1.0f : 1.0f;
            const float headroom = 1.0f - threshold;
            return sign * (threshold + headroom * std::tanh((magnitude - threshold) / headroom));
        }

        float presence_gain_db(float normalized)
        {
            return (normalized - 0.5f) * 2.0f * kMaxPresenceDb;
        }

        // The 2203's preamp volume is a pot between V1a's plate and V1b's grid, so the knob is an
        // attenuator, not an input multiplier: turning it up feeds V1b (and everything after it)
        // harder, which is what produces the cascaded-gain-stage distortion rather than just a
        // louder clean signal.
        //
        // The exponent approximates an audio-taper pot. It is 1.5 rather than a plain square
        // because the cascade's clean-to-saturated transition spans about 26 dB of interstage
        // level (grid conduction sets in around 0.203 V; the cutoff bound is at 4.14 V), so a
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
        , v1a_(kTriodeSourceResistance, kTriodeGridLeak, kTriodeCathodeBias, kTriodeGain, kTriodeSaturationCurrent, kTriodeThermalVoltage)
        , v1b_(kTriodeSourceResistance, kTriodeGridLeak, kTriodeCathodeBias, kTriodeGain, kTriodeSaturationCurrent, kTriodeThermalVoltage)
        , tone_stack_(48000.0)
        , v2a_(kTriodeSourceResistance, kTriodeGridLeak, kTriodeCathodeBias, kTriodeGain, kTriodeSaturationCurrent, kTriodeThermalVoltage)
        , phase_inverter_(kTriodeSourceResistance, kTriodeGridLeak, kTriodeCathodeBias, kPhaseInverterGain, kTriodeSaturationCurrent, kTriodeThermalVoltage)
        , el34_a_(kPentodeSourceResistance, kPentodeGridLeak, kPentodeCathodeBias, kPentodeGain, kPentodeSaturationCurrent, kPentodeThermalVoltage, kPentodePlateSaturationDrive)
        , el34_b_(kPentodeSourceResistance, kPentodeGridLeak, kPentodeCathodeBias, kPentodeGain, kPentodeSaturationCurrent, kPentodeThermalVoltage, kPentodePlateSaturationDrive)
        , output_transformer_(48000.0 * kOversamplingFactor, kOutputTransformerRolloffHz, kOutputTransformerLeakageHz, kOutputTransformerSaturationDrive)
        , sag_(48000.0 * kOversamplingFactor, kSagAmount, kSagRecoveryTimeSeconds)
    {
    }

    void Jcm800::prepare(const PrepareInfo& info)
    {
        sample_rate_ = info.sample_rate;
        bright_cap_.set_high_pass(sample_rate_, kBrightCapHz, kFilterQ);
        // The V1a->V1b coupling capacitor sits inside the oversampled region, so its coefficients
        // must be derived at the oversampled rate; deriving them at the base rate would place the
        // real corner 8x higher (960 Hz instead of 120 Hz) and gut the low end.
        coupling_cap_.set_high_pass(sample_rate_ * kOversamplingFactor, kCouplingCapHz, kFilterQ);
        presence_shelf_.set_high_shelf(sample_rate_, kPresenceHz, presence_gain_db(presence_current_), kFilterQ);

        tone_stack_ = JcmToneStack(sample_rate_);
        tone_stack_.set_bass(bass_normalized_);
        tone_stack_.set_middle(middle_normalized_);
        tone_stack_.set_treble(treble_normalized_);

        // The output transformer and the sag stage run inside the oversampled region (see
        // process()), so both are built at the oversampled rate: their filter corners and the
        // sag recovery time constant are all derived from the rate they are stepped at.
        const double oversampled_rate = sample_rate_ * kOversamplingFactor;
        output_transformer_ = OutputTransformerStage(oversampled_rate, kOutputTransformerRolloffHz, kOutputTransformerLeakageHz, kOutputTransformerSaturationDrive);
        sag_ = PowerSupplySagStage(oversampled_rate, kSagAmount, kSagRecoveryTimeSeconds);

        oversampler_a_ = std::make_unique<Oversampler8x>(info.max_block);
        oversampler_b_ = std::make_unique<Oversampler8x>(info.max_block);
        oversampler_c_ = std::make_unique<Oversampler8x>(info.max_block);

        scratch_base_.assign(info.max_block, 0.0f);
        scratch_oversampled_a_.assign(static_cast<std::size_t>(info.max_block) * kOversamplingFactor, 0.0f);
        scratch_oversampled_b_.assign(static_cast<std::size_t>(info.max_block) * kOversamplingFactor, 0.0f);
        scratch_oversampled_c_.assign(static_cast<std::size_t>(info.max_block) * kOversamplingFactor, 0.0f);

        // The cabinet's partition size is the host's block size, not a fixed constant:
        // PartitionedConvolution is a fixed-block API, so any mismatch would force process() down
        // its bypass path and silently drop the cabinet on every host that isn't running the one
        // hardcoded size. PartitionedConvolution requires a power-of-two partition (its FFT is
        // sized 2*partition_size), so a host with a non-power-of-two block size still gets no
        // cabinet — an accepted limitation, flagged rather than papered over, since resolving it
        // needs an input-side ring buffer to re-block the stream and the latency that implies.
        cabinet_.reset();
        cabinet_partition_size_ = 0;
        const bool block_is_power_of_two =
            info.max_block > 0 && (info.max_block & (info.max_block - 1)) == 0;
        if (!cabinet_impulse_response_.empty() && block_is_power_of_two)
        {
            cabinet_partition_size_ = info.max_block;
            cabinet_ = std::make_unique<PartitionedConvolution>(
                cabinet_partition_size_, cabinet_impulse_response_.data(),
                static_cast<std::uint32_t>(cabinet_impulse_response_.size()));
        }

        reset();
    }

    void Jcm800::process(ProcessContext& context)
    {
        preamp_current_ = slew_toward(preamp_current_, preamp_target_);
        master_current_ = slew_toward(master_current_, master_target_);
        presence_current_ = slew_toward(presence_current_, presence_target_);
        presence_shelf_.set_high_shelf(sample_rate_, kPresenceHz, presence_gain_db(presence_current_), kFilterQ);

        const float* input = context.inputs[0];
        float* output = context.outputs[0];
        const std::uint32_t frames = context.frames;
        const std::uint32_t oversampled_frames = frames * kOversamplingFactor;

        const float preamp_gain = preamp_attenuation(preamp_current_);

        for (std::uint32_t i = 0; i < frames; ++i)
        {
            scratch_base_[i] = bright_cap_.process_sample(input[i]);
        }

        const float* upsampled_a = oversampler_a_->upsample(scratch_base_.data(), frames);
        for (std::uint32_t i = 0; i < oversampled_frames; ++i)
        {
            const float v1a_out = v1a_.process_sample(upsampled_a[i]);
            const float v1b_in = coupling_cap_.process_sample(v1a_out) * preamp_gain;
            scratch_oversampled_a_[i] = v1b_.process_sample(v1b_in) * kV1bToV2aAttenuation;
        }
        oversampler_a_->downsample(scratch_oversampled_a_.data(), oversampled_frames, scratch_base_.data());

        for (std::uint32_t i = 0; i < frames; ++i)
        {
            scratch_base_[i] = tone_stack_.process_sample(scratch_base_[i]);
        }

        const float* upsampled_b = oversampler_b_->upsample(scratch_base_.data(), frames);
        for (std::uint32_t i = 0; i < oversampled_frames; ++i)
        {
            const float phase_split = v2a_.process_sample(upsampled_b[i]) * kV2aToPhaseInverterAttenuation;
            const float phase_inverted =
                phase_inverter_.process_sample(phase_split) * kPhaseInverterToPowerTubeAttenuation;
            const float el34_a_out = el34_a_.process_sample(phase_inverted);
            const float el34_b_out = el34_b_.process_sample(-phase_inverted - kPowerTubeGridBiasMismatchVolts);
            const float power_tube_out = (el34_a_out - el34_b_out) * kPowerTubeToTransformerAttenuation;

            // Both of these stay inside the oversampled region, as spec section 5's diagram
            // places them: the transformer's core saturation is a genuine tanh nonlinearity and
            // the sag stage's gain is signal-dependent, so running either after the decimator
            // would fold their generated harmonics back below Nyquist.
            const float transformer_out = output_transformer_.process_sample(power_tube_out);
            scratch_oversampled_b_[i] = sag_.process_sample(transformer_out);
        }
        oversampler_b_->downsample(scratch_oversampled_b_.data(), oversampled_frames, scratch_base_.data());

        // The presence shelf and the master volume are linear, so they generate nothing to alias
        // and cost 8x less on this side of the decimator.
        for (std::uint32_t i = 0; i < frames; ++i)
        {
            scratch_base_[i] = presence_shelf_.process_sample(scratch_base_[i]) * master_current_;
        }

        // PartitionedConvolution::process() is a fixed-block API with no frame-count argument;
        // feeding it a short final block would read/write past the caller's buffer. Bypass rather
        // than corrupt when the host's block size doesn't match the cabinet's partition size.
        if (cabinet_ && frames == cabinet_partition_size_)
        {
            cabinet_->process(scratch_base_.data(), output);
        }
        else
        {
            for (std::uint32_t i = 0; i < frames; ++i)
            {
                output[i] = scratch_base_[i];
            }
        }

        // The limiter is a genuine waveshaping nonlinearity (like the output transformer's core
        // saturation above), so when it actually engages it must run inside its own oversampled
        // region rather than at the base rate output writes land at above: applying it
        // post-decimation would generate harmonics with nothing left to band-limit them, aliasing
        // straight back down into the audible range - audible as tearing on bright,
        // high-frequency-heavy signals even though the limiter itself only ever nudges the loudest
        // ~1 dB of samples.
        //
        // Below the threshold, soft_limit() is exactly the identity function - but running every
        // block through the oversample/downsample round trip regardless would still change it
        // very slightly (the halfband filters' passband is near-unity, not bit-exact-unity), which
        // is audible to nothing but is enough to disturb this file's THD/regression measurements
        // at ordinary playing levels. So the whole oversampled detour is skipped whenever nothing
        // in this block would actually be touched - the same signal that reaches here untouched
        // leaves untouched too, and the oversampler's history sits idle rather than drifting out
        // of sync with a signal it isn't filtering.
        bool needs_limiting = false;
        for (std::uint32_t i = 0; i < frames; ++i)
        {
            if (std::fabs(output[i]) > kOutputLimiterThreshold)
            {
                needs_limiting = true;
                break;
            }
        }

        if (needs_limiting)
        {
            const float* upsampled_c = oversampler_c_->upsample(output, frames);
            const std::uint32_t oversampled_frames_c = frames * kOversamplingFactor;
            for (std::uint32_t i = 0; i < oversampled_frames_c; ++i)
            {
                scratch_oversampled_c_[i] = soft_limit(upsampled_c[i], kOutputLimiterThreshold);
            }
            oversampler_c_->downsample(scratch_oversampled_c_.data(), oversampled_frames_c, output);
        }
    }

    PortLayout Jcm800::ports() const
    {
        return { 1, 1 };
    }

    void Jcm800::reset()
    {
        bright_cap_.reset();
        coupling_cap_.reset();
        presence_shelf_.reset();
        v1a_.reset();
        v1b_.reset();
        tone_stack_.reset();
        v2a_.reset();
        phase_inverter_.reset();
        el34_a_.reset();
        el34_b_.reset();
        output_transformer_.reset();
        sag_.reset();
        if (oversampler_a_)
        {
            oversampler_a_->reset();
        }
        if (oversampler_b_)
        {
            oversampler_b_->reset();
        }
        if (oversampler_c_)
        {
            oversampler_c_->reset();
        }
    }

    void Jcm800::set_preamp(float normalized) { preamp_target_ = std::clamp(normalized, 0.0f, 1.0f); }
    void Jcm800::set_master(float normalized) { master_target_ = std::clamp(normalized, 0.0f, 1.0f); }
    void Jcm800::set_bass(float normalized)
    {
        bass_normalized_ = std::clamp(normalized, 0.0f, 1.0f);
        tone_stack_.set_bass(bass_normalized_);
    }
    void Jcm800::set_middle(float normalized)
    {
        middle_normalized_ = std::clamp(normalized, 0.0f, 1.0f);
        tone_stack_.set_middle(middle_normalized_);
    }
    void Jcm800::set_treble(float normalized)
    {
        treble_normalized_ = std::clamp(normalized, 0.0f, 1.0f);
        tone_stack_.set_treble(treble_normalized_);
    }
    void Jcm800::set_presence(float normalized) { presence_target_ = std::clamp(normalized, 0.0f, 1.0f); }

    float Jcm800::get_parameter(const char* id) const
    {
        if (std::strcmp(id, "preamp") == 0) return preamp_target_;
        if (std::strcmp(id, "master") == 0) return master_target_;
        if (std::strcmp(id, "bass") == 0) return bass_normalized_;
        if (std::strcmp(id, "middle") == 0) return middle_normalized_;
        if (std::strcmp(id, "treble") == 0) return treble_normalized_;
        if (std::strcmp(id, "presence") == 0) return presence_target_;
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
    }

    namespace
    {
        constexpr ParameterDescriptor kJcm800Parameters[] = {
            { "preamp",   "Preamp",   0.0f, 1.0f, 0.5f, ParamKind::Linear, "" },
            { "master",   "Master",   0.0f, 1.0f, 0.7f, ParamKind::Linear, "" },
            { "bass",     "Bass",     0.0f, 1.0f, 0.5f, ParamKind::Linear, "" },
            { "middle",   "Middle",   0.0f, 1.0f, 0.5f, ParamKind::Linear, "" },
            { "treble",   "Treble",   0.0f, 1.0f, 0.5f, ParamKind::Linear, "" },
            { "presence", "Presence", 0.0f, 1.0f, 0.5f, ParamKind::Linear, "" },
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
            6,
            PortLayout{ 1, 1 },
            &make_jcm800,
        };
        return descriptor;
    }
}
