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

        constexpr float kSmoothingMs = 20.0f;

        // Below this, a recomputed tone-stack/presence coefficient set would be numerically
        // indistinguishable from the one already in place, so the audio thread skips the rebuild.
        // It is a cost filter, not a quality one: 1e-4 of a knob's 0..1 travel is far finer than
        // the ~1e-3 step a 7-bit MIDI controller can even express.
        constexpr float kCoefficientEpsilon = 1e-4f;

        // The master knob's reference point: 0 dB reproduces the linear 0.7 multiplier that was
        // this amp's default before the control became a dB level, so nothing about the shipped
        // loudness moved when the units did.
        constexpr float kMasterUnityGain = 0.7f;

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
        presence_shelf_.set_high_shelf(sample_rate_, kPresenceHz, presence_gain_db(presence_.target()), kFilterQ);

        tone_stack_ = JcmToneStack(sample_rate_);

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

        oversampler_a_ = std::make_unique<Oversampler8x>(info.max_block);
        oversampler_b_ = std::make_unique<Oversampler8x>(info.max_block);

        scratch_base_.assign(info.max_block, 0.0f);
        scratch_oversampled_a_.assign(static_cast<std::size_t>(info.max_block) * kOversamplingFactor, 0.0f);
        scratch_oversampled_b_.assign(static_cast<std::size_t>(info.max_block) * kOversamplingFactor, 0.0f);

        max_block_ = info.max_block;

        // StreamingConvolution re-blocks the stream internally, so the partition is a property of
        // the sample rate alone rather than of the host's block size: every block size, power of
        // two or not, gets the same cabinet and the same one-partition latency.
        cabinet_partition_ = StreamingConvolution::partition_size_for(sample_rate_);
        if (!cabinet_impulse_response_.empty())
        {
            cabinet_slot_.publish(std::make_unique<StreamingConvolution>(
                cabinet_partition_, cabinet_impulse_response_.data(),
                static_cast<std::uint32_t>(cabinet_impulse_response_.size()), max_block_));
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
        const float master_gain = master * kMasterUnityGain;

        for (std::uint32_t i = 0; i < frames; ++i)
        {
            scratch_base_[i] = bright_cap_.process_sample(input[i] * input_gain);
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
        const std::uint32_t oversampling_latency =
            (oversampler_a_ ? oversampler_a_->latency_samples() : 0)
            + (oversampler_b_ ? oversampler_b_->latency_samples() : 0);
        return oversampling_latency + (has_cabinet() ? cabinet_partition_ : 0);
    }

    bool Jcm800::set_cabinet(Ir impulse_response)
    {
        if (impulse_response.samples.empty() || impulse_response.sample_rate != sample_rate_)
        {
            return false;
        }
        // Kept so a later prepare() can rebuild this cabinet at the new partition and block size.
        // RtSwapSlot has no "unpublish", so a cabinet that outlived its prepare() would otherwise
        // stay active with a stale partition and a latency figure that no longer described it.
        cabinet_impulse_response_ = std::move(impulse_response.samples);
        cabinet_slot_.publish(std::make_unique<StreamingConvolution>(
            cabinet_partition_, cabinet_impulse_response_.data(),
            static_cast<std::uint32_t>(cabinet_impulse_response_.size()), max_block_));
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
        // reset() mutates every stage's state, so it is already mutually exclusive with
        // process(); acquiring the slot here therefore cannot race a live audio thread, and the
        // generation it records is the same one the next process() would record anyway.
        if (StreamingConvolution* const cabinet = cabinet_slot_.acquire_for_audio_thread())
        {
            cabinet->reset();
        }
    }

    void Jcm800::set_preamp(float normalized) { preamp_.set_target(std::clamp(normalized, 0.0f, 1.0f)); }
    void Jcm800::set_bass(float normalized) { bass_.set_target(std::clamp(normalized, 0.0f, 1.0f)); }
    void Jcm800::set_middle(float normalized) { middle_.set_target(std::clamp(normalized, 0.0f, 1.0f)); }
    void Jcm800::set_treble(float normalized) { treble_.set_target(std::clamp(normalized, 0.0f, 1.0f)); }
    void Jcm800::set_presence(float normalized) { presence_.set_target(std::clamp(normalized, 0.0f, 1.0f)); }

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
