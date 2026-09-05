/**************************************************************************/
/* Dd3Delay.hpp                                                           */
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
 * @file Dd3Delay.hpp
 * @brief The DD-3 node: the variable-rate loop closed around all four DD-3 blocks.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <array>
#include <cstdint>

#include "SushiDSP/core/IAudioNode.hpp"
#include "SushiDSP/core/NodeDescriptor.hpp"
#include "SushiDSP/dd3/Dd3AnalogFilters.hpp"
#include "SushiDSP/dd3/Dd3Clock.hpp"
#include "SushiDSP/dd3/Dd3Converter.hpp"
#include "SushiDSP/dd3/Ne570Compander.hpp"
#include "SushiDSP/math/Resampler.hpp"
#include "SushiDSP/math/SmoothedParameter.hpp"

namespace SushiDSP
{
    /// @brief The Boss DD-3 delay: emphasis, anti-alias, compander, 12-bit ring and clock,
    /// with an analog feedback path wrapping the whole chain.
    ///
    /// The host rate reaches the converter through two Resamplers whose ratios follow the
    /// D.TIME clock every @ref kClockUpdateFrames samples. The feedback leaves the wet leg's
    /// output and is summed one sample at a time ahead of the anti-alias filter, so the
    /// emphasis pair cancels inside the loop; the ruling behind that take-off is in
    /// docs/superpowers/specs/2026-09-05-dd3-spx90-design.md, Model corrections.
    class Dd3Delay : public IAudioNode
    {
    public:
        /// @brief Puts every knob at its descriptor default and hands D.TIME to the clock.
        Dd3Delay();

        /// @brief Places every filter, sizes both Resamplers and allocates the delay ring.
        /// @param info Sample rate, max block size, and channel count for this node's lifetime.
        void prepare(const PrepareInfo& info) override;

        /// @brief Runs one block through the loop, re-clocking every kClockUpdateFrames samples.
        /// @param context The per-block input/output buffers and frame count.
        void process(ProcessContext& context) override;

        /// @brief Returns this node's fixed input/output channel counts.
        /// @return Mono in, mono out.
        PortLayout ports() const override;

        /// @brief Clears every filter, the ring, both Resamplers and the wet FIFO.
        void reset() override;

        /// @brief Latency a host would have to compensate.
        /// @return 0 always: the dry path is sample-aligned and the wet delay is the effect
        /// (plan Ruling 5).
        std::uint32_t latency_samples() const override;

        /// @brief This pedal's registered type id.
        /// @return "sushidsp.dd3".
        const char* type_id() const override;

        /// @brief Sets D.TIME, which the clock's own R56/C52 pole then glides toward.
        /// @param normalized 0 = fastest clock and shortest delay, 1 = slowest.
        void set_time(float normalized);

        /// @brief Sets F.BACK, smoothed toward this value over subsequent samples.
        /// @param normalized Regeneration amount in [0, 1]; 1 is just past self-oscillation.
        void set_feedback(float normalized);

        /// @brief Sets E.LEVEL, smoothed toward this value over subsequent samples.
        /// @param normalized Wet level in [0, 1]; 0 makes the node a wire.
        void set_level(float normalized);

        /// @brief Selects the delay range, or HOLD. Control thread only.
        /// @param mode Short, Middle, Long or Hold; Long and Hold share N, so the loop survives
        /// the switch into HOLD.
        void set_mode(dd3::Dd3Mode mode);

        using IAudioNode::set_parameter;

        /// @brief Reads a parameter by string id.
        /// @param id One of "time", "feedback", "level", "mode".
        /// @return The parameter's target value, or 0 for an unknown id.
        float get_parameter(const char* id) const override;

        /// @brief Sets a parameter by string id.
        /// @param id One of "time", "feedback", "level", "mode".
        /// @param value The new target value, in that parameter's declared range.
        void set_parameter(const char* id, float value) override;

        /// @brief Sets a parameter by descriptor index, RT-safe and free of string compares.
        /// @param index The parameter's index in dd3_descriptor().
        /// @param value The new target value, in that parameter's declared range.
        void set_parameter(std::uint16_t index, float value) override;

        /// @brief The wet path's current delay: the ring, plus the two conversions' round trip,
        /// plus the wet FIFO's prime (plan N2). Not reported as node latency, because the dry
        /// path is sample-aligned and the wet delay is the effect.
        /// @return Seconds.
        /// @pre prepare() has been called; on an unprepared node this returns 0.
        double wet_delay_seconds() const;

        /// @brief Samples between clock updates; the R56/C52 pole advances 24.47 % per block
        /// of this length at 48 kHz (plan Ruling 11).
        static constexpr std::uint32_t kClockUpdateFrames = 8u;

        /// @brief Wet samples the FIFO holds back before it starts delivering, so that the 0, 1
        /// or 2 samples one base-rate push returns never starve the read (plan N2).
        ///
        /// The prime is taken on the first samples the loop actually produces, not written as
        /// zeros at reset; the measurement behind that is in
        /// .superpowers/sdd/2026-09-05-dr1-dr2-resampler-and-dd3/task-2.5-report.md.
        static constexpr std::uint32_t kWetPrimeFrames = 8u;

        /// @brief Input-path gain that reproduces the service note's check point [1].
        ///
        /// A calibration residual, not a value derived from any component: the transcribed
        /// pre-emphasis stage turns the note's 100 mVp-p into 131.837 mVp-p where the note
        /// prints 620, and 620 / 131.837 = 4.70358 closes the gap (plan N7).
        static constexpr double kInputPathGainToCheckPointOne = 4.70358;

        /// @brief Fixed gain of the expander's output network, R42 47K with C20 0.0027 || R41 22K.
        ///
        /// Check points [3] 2.3 Vp-p and [4] 2.4 Vp-p: the expander returns 1.15^2 / 6.6125 =
        /// 0.2 V peak = 0.4 Vp-p, and 2.4 / 0.4 = 6.0 (plan N7).
        static constexpr double kExpanderOutputStageGain = 6.0;

        /// @brief Loop gain at the top of the F.BACK knob; above 1, so the pedal self-oscillates
        /// there, as it does on the bench.
        static constexpr double kMaximumFeedback = 1.1;

        /// @brief Branch gain of the F.BACK pot into the pre-emphasis summing node: R12 33K
        /// over VR2 50 kB at the top of the pot (dossier 1.4 and 1.6).
        static constexpr double kFeedbackMixRatio = 33.0 / 50.0;

        /// @brief What the mix resistors leave unexplained in the loop's forward gain.
        ///
        /// The take-off is at the wet leg's output, so the emphasis pair cancels inside the
        /// loop and the forward gain is kInputPathGainToCheckPointOne *
        /// kExpanderOutputStageGain * |anti-alias| * |reconstruction|, which peaks at
        /// 29.719044 at 3278 Hz and is that same number at 48, 96 and 192 kHz. Against
        /// kFeedbackMixRatio that leaves 19.614569, a residual fit like
        /// kInputPathGainToCheckPointOne; dossier 4.7 item 5 replaces it.
        static constexpr double kFeedbackResidualTrim = 1.0 / 19.614569;

        /// @brief Attenuation from the F.BACK take-off node into the summing node, which makes
        /// kMaximumFeedback the loop gain it says it is.
        static constexpr double kFeedbackPathGain = kFeedbackMixRatio * kFeedbackResidualTrim;

        /// @brief Wet gain at E.LEVEL fully up, from the service note's check point [5].
        ///
        /// The note's own stimulus is 1 kHz at 100 mVp-p, i.e. 0.025 full scale. In mode M at
        /// D.TIME 0.5 it measures 0.713838 full scale of wet at unity, and [5] reads 140 mVp-p,
        /// i.e. 0.035 full scale, so 0.035 / 0.713838 = 0.0490307.
        static constexpr float kLevelUnityGain = 0.0490307f;

    private:
        /// @brief Runs one base-rate sample through the whole loop.
        /// @param input The dry sample in normalized units.
        /// @return The mixed output sample in normalized units.
        float process_sample(float input);

        /// @brief Carries one base-rate sample through the converter's clock and back.
        /// @param drive The compressed sample at the host rate, in volts.
        /// @return The delayed sample at the host rate, in volts.
        float convert_sample(float drive);

        /// @brief Appends one host-rate wet sample, dropping the oldest if the ring is full.
        /// @param sample The sample to append.
        void push_wet(float sample);

        /// @brief Takes the oldest wet sample once the prime has been reached.
        /// @return The sample, or 0 while priming or starved.
        float pop_wet();

        /// @brief Most fs-rate samples one base-rate push can produce, with headroom.
        static constexpr std::uint32_t kFsScratchCapacity = 16u;

        /// @brief Most base-rate samples one fs-rate push can produce, with headroom.
        static constexpr std::uint32_t kWetScratchCapacity = 16u;

        /// @brief Wet FIFO length; a power of two, so free-running indices mask onto it.
        static constexpr std::uint32_t kWetFifoCapacity = 64u;

        /// @brief Mask that wraps a free-running wet FIFO index onto kWetFifoCapacity.
        static constexpr std::uint32_t kWetFifoMask = kWetFifoCapacity - 1u;

        double sample_rate_ = 48000.0;
        bool   prepared_    = false;

        float             time_ = 0.3f;
        SmoothedParameter feedback_{ 0.3f };
        SmoothedParameter level_{ 0.5f };
        dd3::Dd3Mode      mode_ = dd3::Dd3Mode::Middle;

        dd3::Dd3PreEmphasis         pre_emphasis_;
        dd3::Dd3AntiAliasFilter     anti_alias_;
        dd3::Ne570Compander         compressor_{ dd3::CompanderHalf::Compressor };
        dd3::Dd3Converter           converter_;
        dd3::Ne570Compander         expander_{ dd3::CompanderHalf::Expander };
        dd3::Dd3ReconstructionFilter reconstruction_;
        dd3::Dd3DeEmphasis          de_emphasis_;
        dd3::Dd3Clock               clock_;

        Resampler down_;                         // host rate to fs; minimum ratio 20 kHz / host
        Resampler up_;                           // fs back to host; minimum ratio host / 82.5 kHz

        std::array<float, kFsScratchCapacity>  fs_scratch_{};
        std::array<float, kWetScratchCapacity> wet_scratch_{};
        std::array<float, kWetFifoCapacity>    wet_fifo_{};
        std::uint32_t wet_write_ = 0u;           // free-running; kWetFifoCapacity divides 2^32
        std::uint32_t wet_read_  = 0u;
        bool          wet_priming_ = true;       // withholding wet output until the prime is met

        double wet_previous_ = 0.0;              // the wet leg's output, in volts; the take-off
    };

    /// @brief The DD-3's product registration entry point.
    /// @return The static NodeDescriptor for "sushidsp.dd3".
    const NodeDescriptor& dd3_descriptor();
}
