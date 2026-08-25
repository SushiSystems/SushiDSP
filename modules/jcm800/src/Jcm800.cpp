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
        constexpr float kPentodeCathodeBiasA = -20.0f;
        constexpr float kPentodeCathodeBiasB = -20.5f;  // small asymmetry: the class-AB bias offset
        constexpr float kPentodeGain = 8.0f;
        constexpr float kPentodeSaturationCurrent = 1e-9f;
        constexpr float kPentodeThermalVoltage = 0.02585f;
        constexpr float kPentodePlateSaturationDrive = 0.05f;

        constexpr float kOutputTransformerRolloffHz = 60.0f;
        constexpr float kOutputTransformerLeakageHz = 8000.0f;
        constexpr float kOutputTransformerSaturationDrive = 0.3f;

        constexpr float kSagAmount = 0.3f;
        constexpr float kSagRecoveryTimeSeconds = 0.05f;

        // Interstage gain staging. Each triode's plate swings +/- gain * Vgrid, i.e. tens of
        // volts, while the next grid can only accept a fraction of a volt before it either
        // conducts (positive half) or hits the Newton-Raphson bound (negative half). A real amp
        // resolves this with the plate-load/grid-leak divider and coupling network at every
        // interstage link; without those dividers the four cascaded stages multiply to a gain of
        // 60*60*60*40 ~ 8.6e6 and every stage past V1a sits permanently against its solver bound,
        // which makes the whole amp a fixed-amplitude square-wave generator with a dead gain
        // knob. The first four attenuations below stand in for those dividers and keep each grid
        // at roughly a third of the +/-60*Vt (1.551 V) bound at nominal playing levels — see the
        // Task 7 report's Newton-Raphson ruling. kPowerTubeToTransformerAttenuation is different:
        // it sits after the EL34 plates, so it is an output-level trim into the transformer, not a
        // grid-bound protection constant, and isn't part of that ruling's headroom argument.
        constexpr float kPreampPotMaxAttenuation = 1.0f / 60.0f;
        constexpr float kV1bToV2aAttenuation = 1.0f / 60.0f;
        constexpr float kV2aToPhaseInverterAttenuation = 1.0f / 60.0f;
        constexpr float kPhaseInverterToPowerTubeAttenuation = 1.0f / 20.0f;
        constexpr float kPowerTubeToTransformerAttenuation = 0.1f;

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

        float presence_gain_db(float normalized)
        {
            return (normalized - 0.5f) * 2.0f * kMaxPresenceDb;
        }

        // The 2203's preamp volume is a pot between V1a's plate and V1b's grid, so the knob is an
        // attenuator, not an input multiplier: turning it up feeds V1b (and everything after it)
        // harder, which is what produces the cascaded-gain-stage distortion rather than just a
        // louder clean signal. The square approximates the audio taper such a pot is made with.
        float preamp_attenuation(float normalized)
        {
            return normalized * normalized * kPreampPotMaxAttenuation;
        }
    }

    Jcm800::Jcm800(std::vector<float> cabinet_impulse_response)
        : cabinet_impulse_response_(std::move(cabinet_impulse_response))
        , v1a_(kTriodeSourceResistance, kTriodeGridLeak, kTriodeCathodeBias, kTriodeGain, kTriodeSaturationCurrent, kTriodeThermalVoltage)
        , v1b_(kTriodeSourceResistance, kTriodeGridLeak, kTriodeCathodeBias, kTriodeGain, kTriodeSaturationCurrent, kTriodeThermalVoltage)
        , tone_stack_(48000.0)
        , v2a_(kTriodeSourceResistance, kTriodeGridLeak, kTriodeCathodeBias, kTriodeGain, kTriodeSaturationCurrent, kTriodeThermalVoltage)
        , phase_inverter_(kTriodeSourceResistance, kTriodeGridLeak, kTriodeCathodeBias, kPhaseInverterGain, kTriodeSaturationCurrent, kTriodeThermalVoltage)
        , el34_a_(kPentodeSourceResistance, kPentodeGridLeak, kPentodeCathodeBiasA, kPentodeGain, kPentodeSaturationCurrent, kPentodeThermalVoltage, kPentodePlateSaturationDrive)
        , el34_b_(kPentodeSourceResistance, kPentodeGridLeak, kPentodeCathodeBiasB, kPentodeGain, kPentodeSaturationCurrent, kPentodeThermalVoltage, kPentodePlateSaturationDrive)
        , output_transformer_(48000.0, kOutputTransformerRolloffHz, kOutputTransformerLeakageHz, kOutputTransformerSaturationDrive)
        , sag_(48000.0, kSagAmount, kSagRecoveryTimeSeconds)
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

        output_transformer_ = OutputTransformerStage(sample_rate_, kOutputTransformerRolloffHz, kOutputTransformerLeakageHz, kOutputTransformerSaturationDrive);
        sag_ = PowerSupplySagStage(sample_rate_, kSagAmount, kSagRecoveryTimeSeconds);

        oversampler_a_ = std::make_unique<Oversampler8x>(info.max_block);
        oversampler_b_ = std::make_unique<Oversampler8x>(info.max_block);

        scratch_base_.assign(info.max_block, 0.0f);
        scratch_oversampled_a_.assign(static_cast<std::size_t>(info.max_block) * kOversamplingFactor, 0.0f);
        scratch_oversampled_b_.assign(static_cast<std::size_t>(info.max_block) * kOversamplingFactor, 0.0f);

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
            const float el34_b_out = el34_b_.process_sample(-phase_inverted);
            scratch_oversampled_b_[i] = (el34_a_out - el34_b_out) * kPowerTubeToTransformerAttenuation;
        }
        oversampler_b_->downsample(scratch_oversampled_b_.data(), oversampled_frames, scratch_base_.data());

        for (std::uint32_t i = 0; i < frames; ++i)
        {
            float x = output_transformer_.process_sample(scratch_base_[i]);
            x = sag_.process_sample(x);
            x = presence_shelf_.process_sample(x);
            scratch_base_[i] = x * master_current_;
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
