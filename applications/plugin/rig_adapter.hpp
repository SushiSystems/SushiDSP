/**************************************************************************/
/* rig_adapter.hpp                                                        */
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
 * @file rig_adapter.hpp
 * @brief Declares the format-agnostic adapter a plugin shell drives one rig through.
 * @author Mustafa Garip
 */

#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "SushiDSP/core/audio_node.hpp"
#include "SushiDSP/core/rt_swap_slot.hpp"

#include "parameter_routing.hpp"
#include "rig_parameter.hpp"

class RigController;

namespace SushiDSP::Plugin
{
    /// @brief Says whether the adapter took a state or a format it was handed.
    enum class RigStateStatus : std::uint8_t
    {
        Ok,     /**< The adapter holds the rig, or wrote it. */
        Refused /**< The rig stands as it was; the error text says why. */
    };

    /// @brief Runs one rig for a plugin shell and knows no plugin format. It prepares at a rate
    /// and a block size, loads and saves the rig preset the standalone host writes, lists the
    /// rig's parameters and renders two channels. Every member belongs to the control thread
    /// unless its name ends in for_audio_thread or it is process().
    /// @see applications/plugin/README.md
    class RigAdapter final
    {
        public:
            /// @brief Counts the channels process() reads and writes.
            static constexpr std::uint32_t K_CHANNELS = 2;

            /// @brief Names the rate the rig runs at until prepare() gives it another.
            static constexpr double K_DEFAULT_SAMPLE_RATE = 48000.0;

            /// @brief Names the block size the rig runs at until prepare() gives it another.
            static constexpr std::uint32_t K_DEFAULT_MAX_BLOCK_FRAMES = 1024;

            /// @brief Bounds the parameter changes the audio thread holds between two blocks.
            static constexpr std::size_t K_PENDING_CAPACITY = 1024;

            /// @brief Builds an empty rig at the default rate and block size.
            RigAdapter();

            RigAdapter(const RigAdapter&) = delete;
            RigAdapter& operator=(const RigAdapter&) = delete;
            RigAdapter(RigAdapter&&) = delete;
            RigAdapter& operator=(RigAdapter&&) = delete;
            ~RigAdapter();

            /// @brief Rebuilds the rig it holds at a rate and a block size.
            /// @param sample_rate The rate in Hz; above zero.
            /// @param max_block_frames The most frames one process() call carries; above zero.
            /// @param error_out Receives the reason when the format is refused; "" otherwise.
            /// @pre No process() call is running or starts before this returns.
            [[nodiscard]] RigStateStatus prepare(double sample_rate, std::uint32_t max_block_frames,
                                                 std::string& error_out);

            /// @brief Replaces the rig from a rig preset. process() may be running.
            /// @param json_text The preset text RigController writes.
            /// @param error_out Receives the reason when the preset is refused; "" otherwise.
            [[nodiscard]] RigStateStatus load_state(const std::string& json_text,
                                                    std::string& error_out);

            /// @brief Writes the rig as a rig preset, with the values the audio thread last set.
            /// @param json_text_out Receives the preset text.
            /// @param error_out Receives the reason when the rig will not serialize; "" otherwise.
            [[nodiscard]] RigStateStatus save_state(std::string& json_text_out,
                                                    std::string& error_out);

            /// @brief Reports the delay the rig adds between its input and its output.
            /// @return The latency in frames at the prepared rate; 0 for an empty rig.
            [[nodiscard]] std::uint32_t latency_frames() const;

            /// @brief Lists the rig's parameters, the slots in rig order. A load_state() or a
            /// prepare() replaces the list.
            [[nodiscard]] const std::vector<RigParameter>& parameters() const noexcept;

            /// @brief Reads a parameter's current value, the audio thread's last write included.
            /// @param id An id from parameters().
            /// @return The value, or std::nullopt when the rig holds no such parameter.
            [[nodiscard]] std::optional<float> parameter_value(std::uint32_t id) const;

            /// @brief Sets a parameter from the control thread, clamped to its range.
            /// @param id An id from parameters().
            /// @return True when the rig holds the parameter.
            bool set_parameter(std::uint32_t id, float value);

            /// @brief Frees the graphs and tables the audio thread has moved past.
            void reclaim();

            /// @brief Counts the audio thread's parameter changes dropped at K_PENDING_CAPACITY.
            [[nodiscard]] std::size_t dropped_parameter_changes() const noexcept;

            /// @brief Holds a parameter change for the next flush or block. Audio thread only;
            /// allocates nothing and locks nothing.
            /// @param id An id from parameters(); an id the rig does not hold is dropped later.
            void queue_parameter_for_audio_thread(std::uint32_t id, float value) noexcept;

            /// @brief Applies the held parameter changes without rendering. Audio thread only.
            void flush_parameters_for_audio_thread() noexcept;

            /// @brief Applies the held parameter changes, then renders one block through the rig.
            /// An empty rig copies each input channel to its output channel. Audio thread only;
            /// allocates nothing and locks nothing. The buffers may be the same memory.
            /// @param channels Must equal K_CHANNELS; any other count clears the outputs.
            /// @param frames At most the prepared block size; a longer block clears the outputs.
            void process(const float* const* inputs, float* const* outputs, std::uint32_t channels,
                         std::uint32_t frames) noexcept;

        private:
            /// @brief Names the controller's slot and parameter behind one listed parameter.
            struct Binding;

            /// @brief Holds one parameter change the audio thread has not applied yet.
            struct PendingChange
            {
                    std::uint32_t id = 0; ///< Holds the parameter's id.
                    float value = 0.0f;   ///< Holds the value to set.
            };

            /// @brief Rebuilds parameters(), the bindings and the routing from the controller's
            /// rig and publishes the routing to the audio thread.
            void publish_routing();

            /// @brief Writes every value the audio thread set into the controller's own state.
            void pull_audio_thread_values();

            /// @brief Applies the held changes to the routing and, when it matches the graph, to
            /// the nodes. Audio thread only.
            void apply_pending(RigGraph* graph, ParameterRouting* routing) noexcept;

            /// @brief Holds the rate, block size and width the controller was built at.
            PrepareInfo info_;

            /// @brief Owns the rig, its preset codec and the graph published to the audio thread.
            std::unique_ptr<RigController> controller_;

            /// @brief Holds the parameter list a shell reads.
            std::vector<RigParameter> parameters_;

            /// @brief Names the controller's slot and parameter behind each entry of parameters_.
            std::vector<Binding> bindings_;

            /// @brief Publishes the routing that belongs to the controller's active graph.
            RtSwapSlot<ParameterRouting> routing_;

            /// @brief Holds the block's inputs, channel after channel, away from the outputs.
            std::vector<float> input_storage_;

            /// @brief Holds the audio thread's parameter changes until the next flush.
            std::array<PendingChange, K_PENDING_CAPACITY> pending_;

            /// @brief Counts the entries of pending_ in use; the audio thread's own.
            std::size_t pending_count_ = 0;

            /// @brief Counts the changes dropped because pending_ was full.
            std::atomic<std::size_t> dropped_parameter_changes_{0};
    };
} // namespace SushiDSP::Plugin
