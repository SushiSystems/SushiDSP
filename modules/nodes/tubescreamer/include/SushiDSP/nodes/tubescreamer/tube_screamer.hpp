/**************************************************************************/
/* tube_screamer.hpp                                                      */
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
 * @file tube_screamer.hpp
 * @brief Declares the Tube Screamer overdrive pedal node and its descriptor.
 * @author Mustafa Garip
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include <SushiDSP/foundation/core/calibration.hpp>
#include <SushiDSP/foundation/core/audio_node.hpp>
#include <SushiDSP/foundation/core/node_descriptor.hpp>
#include <SushiDSP/foundation/core/parameter_table.hpp>
#include <SushiDSP/foundation/math/oversampler.hpp>
#include <SushiDSP/foundation/math/smoothed_parameter.hpp>
#include <SushiDSP/nodes/tubescreamer/tube_screamer_circuits.hpp>

namespace SushiDSP
{
    /// @brief The Tube Screamer overdrive pedal: the ts_clip and ts_tone DK blocks cascaded
    /// inside one 4x-oversampled region, then output level. Exposes tube_screamer_descriptor().
    class TubeScreamer : public AudioNode
    {
        public:
            /// @brief Constructs the pedal at its default knob positions; prepare() builds the blocks.
            TubeScreamer();

            /// @brief Derives both DK blocks at the run rate and sizes the oversampled scratch.
            /// @param info Sample rate, max block size, and channel count for this node's lifetime.
            void prepare(const PrepareInfo& info) override;

            /// @brief Runs one block through the clip block, the tone block and the level gain.
            /// @param context The per-block input/output buffers and frame count.
            void process(ProcessContext& context) override;

            /// @brief Returns this node's fixed input/output channel counts.
            /// @return Mono in, mono out.
            PortLayout ports() const override;

            /// @brief Clears both DK blocks and the oversampler for a click-free restart.
            void reset() override;

            /// @brief The delay a host can compensate: the 4x oversampler's round-trip. The DK
            /// blocks' own group delay is frequency- and tone-dependent, so it is tone, not latency,
            /// and is deliberately left out - the impulse peak sits a couple of samples later.
            /// @return The oversampler's round-trip group delay in base-rate samples; 0 before
            /// prepare(), when no oversampler exists yet.
            std::uint32_t latency_samples() const override;

            /// @brief This pedal's registered type id.
            /// @return "sushidsp.tubescreamer".
            const char* type_id() const override;

            /// @brief Sets the drive control, smoothed toward this value over subsequent samples.
            /// @param normalized Drive amount in [0, 1].
            void set_drive(float normalized);

            /// @brief Sets the tone control, smoothed toward this value over subsequent samples.
            /// @param normalized Tone position in [0, 1] (0 = dark, 1 = bright).
            void set_tone(float normalized);

            /// @brief Sets the output level, smoothed toward this value in the gain domain.
            /// @param db Output level in [-60, +12] dB; 0 dB is the pedal's unity-ish output.
            void set_level(float db);

            /// @brief Sets the trim applied to the input before the clip block's coupling capacitor.
            /// @param db Input trim in [-24, +24] dB.
            void set_input_trim(float db);

            /// @brief Reads a parameter by string id.
            /// @param id One of "drive", "tone", "level", "input_trim".
            /// @return The parameter's target value, or 0 for an unknown id.
            float get_parameter(const char* id) const override;

            /// @brief Sets a parameter by string id.
            /// @param id One of "drive", "tone", "level", "input_trim".
            /// @param value The new target value, in that parameter's own units.
            void set_parameter_for_control_thread(const char* id, float value) override;

            /// @brief Sets a parameter by descriptor index, RT-safe and free of string compares.
            /// @param index The parameter's index in tube_screamer_descriptor().
            /// @param value The new target value, in that parameter's own units.
            void set_parameter_for_audio_thread(std::uint16_t index, float value) override;

            /// @brief This node's parameters, one row each, feeding the descriptor and both
            /// dispatches. Declared on the class rather than at namespace scope so a row's
            /// lambdas can reach the private field a knob actually lives in.
            /// @return The table, built once on first use.
            static const ParameterTable<TubeScreamer, 4>& parameters();

            /// @brief Linear output gain at 0 dB on the Level knob, a -3.17 dB trim. Re-calibrated in
            /// D6c against the measured DK chain at 4x, where a -12 dBFS 82 Hz sine came out at
            /// -7.94 dBFS; D5b's 1.1078783 belonged to the memoryless clipper and the low-pass /
            /// high-pass crossfade that stood in for the tone control before D6c netlisted it. It
            /// multiplies the signal after the last nonlinearity, so it scales the output and
            /// nothing else.
            static constexpr float K_LEVEL_UNITY_GAIN = 0.6942987f;

            /// @brief The endpoints of the drive rheostat in tube_screamer_circuits.hpp: the feedback
            /// resistance the clip block presents at Drive 0 and at Drive 1. Public so a host or a
            /// test can read what the Drive knob's ends mean in ohms without opening the netlist.
            static constexpr float K_MIN_FEEDBACK_RESISTANCE = 51000.0f;
            static constexpr float K_MAX_FEEDBACK_RESISTANCE = 551000.0f;

        private:
            double sample_rate_ = 48000.0;

            SmoothedParameter drive_{0.5f};
            SmoothedParameter tone_{0.5f};
            SmoothedParameter level_gain_{1.0f};
            SmoothedParameter input_trim_gain_{1.0f};
            float level_db_ = 0.0f;
            float input_trim_db_ = 0.0f;

            // The plain port nulls deeper at the 4x rate; see modules/nodes/tubescreamer/README.md.
            tube_screamer_circuits::ClipCircuit clip_circuit_;
            tube_screamer_circuits::DiodePairPort clip_port_;
            tube_screamer_circuits::ToneCircuit tone_circuit_;
            tube_screamer_circuits::LinearPort tone_port_;
            std::size_t drive_pot_ = 0;
            std::size_t tone_pot_ = 0;
            bool circuits_ready_ = false;

            std::unique_ptr<Oversampler4x> oversampler_;
            std::vector<float> oversampled_scratch_;
            std::vector<float> clipped_scratch_;
    };

    /// @brief The Tube Screamer's product registration entry point.
    /// @return The static NodeDescriptor for "sushidsp.tubescreamer".
    const NodeDescriptor& tube_screamer_descriptor();
} // namespace SushiDSP
