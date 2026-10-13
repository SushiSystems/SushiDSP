/**************************************************************************/
/* microphone_node.hpp                                                    */
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
 * @file microphone_node.hpp
 * @brief Declares the microphone rig element: pattern, proximity, polarity and the room flag.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <cstdint>

#include "SushiDSP/core/audio_node.hpp"
#include "SushiDSP/core/node_descriptor.hpp"
#include "SushiDSP/core/parameter_table.hpp"

namespace SushiDSP
{
    /// @brief One microphone as a rig element. A room places it and computes what it hears; this
    /// node carries the gear's own settings and passes the room's signal on, inverted when its
    /// polarity is reversed.
    ///
    /// The pattern and the proximity switch are gear data the room's solve reads; the node only
    /// stores them. The room flag marks an overhead microphone, which a rig sums on its Room strip.
    class MicrophoneNode final : public AudioNode
    {
        public:
            /// @brief Names the capsule's first-order polar pattern, in the order the "pattern"
            /// parameter counts them.
            enum class Pattern : std::uint8_t
            {
                Omni = 0,
                Cardioid,
                Supercardioid,
                FigureEight
            };

            /// @brief Constructs a cardioid with its proximity effect on, normal polarity and no
            /// room flag.
            MicrophoneNode();

            /// @brief Holds no per-rate state; present to satisfy AudioNode.
            /// @param info The rate and block size the node will process at.
            void prepare(const PrepareInfo& info) override;

            /// @brief Copies the room's signal to the output, inverted when polarity is reversed.
            /// @param context The block's input/output buffers and frame count.
            void process(ProcessContext& context) override;

            /// @brief Returns one channel in and one channel out.
            /// @return The port layout.
            PortLayout ports() const override;

            /// @brief Holds no state a reset must clear.
            void reset() override;

            /// @brief Reads a parameter by string id.
            /// @param id One of "pattern", "proximity", "polarity" or "room".
            /// @return The value, or 0.0f for an unknown id.
            float get_parameter(const char* id) const override;

            /// @brief Sets a parameter by string id.
            /// @param id One of "pattern", "proximity", "polarity" or "room".
            /// @param value The pattern's index, or 0.5 and above to set a switch.
            void set_parameter_for_control_thread(const char* id, float value) override;

            /// @brief Sets a parameter by descriptor index.
            /// @param index The parameter's index in the descriptor table.
            /// @param value The pattern's index, or 0.5 and above to set a switch.
            void set_parameter_for_audio_thread(std::uint16_t index, float value) override;

            /// @brief This microphone's registered type id.
            /// @return "sushidsp.microphone".
            const char* type_id() const override;

            /// @brief This node's parameters, one row each.
            /// @return The table, built once on first use.
            static const ParameterTable<MicrophoneNode, 4>& parameters();

            /// @brief Reads the capsule's polar pattern.
            /// @return The pattern.
            Pattern pattern() const noexcept;

            /// @brief Reports whether the capsule's gradient share takes the proximity effect.
            /// @return True when proximity is on.
            bool proximity() const noexcept;

            /// @brief Reports whether this microphone is an overhead (room) microphone.
            /// @return True when the room flag is set.
            bool is_room_microphone() const noexcept;

        private:
            /// @brief Stores the capsule's polar pattern.
            Pattern pattern_ = Pattern::Cardioid;

            /// @brief True when the capsule's gradient share takes the proximity effect.
            bool proximity_ = true;

            /// @brief True when the signal is inverted.
            bool polarity_inverted_ = false;

            /// @brief True when the microphone is an overhead (room) microphone.
            bool room_ = false;
    };

    /// @brief The microphone's registration entry point.
    /// @return The static NodeDescriptor for "sushidsp.microphone".
    const NodeDescriptor& microphone_descriptor();
} // namespace SushiDSP
