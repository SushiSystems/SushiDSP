/**************************************************************************/
/* rig_adapter.cpp                                                        */
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
 * @file rig_adapter.cpp
 * @brief Implements the format-agnostic adapter over the host's RigController.
 * @author Mustafa Garip
 */

#include "rig_adapter.hpp"

#include <algorithm>
#include <cstring>
#include <exception>
#include <utility>

#include <SushiDSP/foundation/core/node_descriptor.hpp>
#include <SushiDSP/foundation/core/rig_graph.hpp>
#include <SushiDSP/catalog/node_registry/node_registry.hpp>

#include "rig_controller.hpp"

namespace SushiDSP::Plugin
{
    /// @brief Names the controller's slot and parameter behind one listed parameter.
    struct RigAdapter::Binding
    {
            RigController::SlotId slot = 0;     ///< Holds the controller's id for the slot.
            const char* parameter_id = nullptr; ///< Names the parameter in its descriptor.
    };

    namespace
    {
        /// @brief Finds a product's descriptor in the node registry.
        /// @return The descriptor, or nullptr when the registry holds no such product.
        const NodeDescriptor* find_descriptor(const std::string& descriptor_id)
        {
            for (const NodeEntry& entry : node_registry())
            {
                if (descriptor_id == entry.descriptor->id)
                {
                    return entry.descriptor;
                }
            }
            return nullptr;
        }

        /// @brief Pairs each slot that is not bypassed with its node in the published graph.
        /// @param order The controller's slots, in rig order.
        /// @return One handle per live slot in order, or an empty list when the graph's nodes
        /// do not line up with the live slots one for one.
        std::vector<NodeHandle>
        handles_of_live_slots(const RigController& controller, const RigGraph* graph,
                              const std::vector<RigController::SlotId>& order)
        {
            if (graph == nullptr)
            {
                return {};
            }
            std::vector<NodeHandle> handles = graph->nodes();
            std::size_t live = 0;
            for (RigController::SlotId slot : order)
            {
                if (controller.is_bypassed(slot))
                {
                    continue;
                }
                if (live == handles.size())
                {
                    return {};
                }
                // SlotGraph adds a node per live slot in order; the type check holds it to that.
                const char* type = graph->node(handles[live])->type_id();
                if (type[0] != '\0' && controller.descriptor_id(slot) != type)
                {
                    return {};
                }
                ++live;
            }
            if (live != handles.size())
            {
                return {};
            }
            return handles;
        }

        /// @brief Counts the parameters of a product a parameter id can address.
        std::uint32_t addressable_parameters(const NodeDescriptor* descriptor)
        {
            if (descriptor == nullptr)
            {
                return 0;
            }
            return static_cast<std::uint32_t>(
                std::min<std::size_t>(descriptor->parameter_count, K_PARAMETER_ID_SPAN));
        }

        /// @brief Clears every output channel of a block.
        void clear_outputs(float* const* outputs, std::uint32_t channels,
                           std::uint32_t frames) noexcept
        {
            for (std::uint32_t channel = 0; channel < channels; ++channel)
            {
                std::memset(outputs[channel], 0, frames * sizeof(float));
            }
        }
    } // namespace

    RigAdapter::RigAdapter()
        : info_{K_DEFAULT_SAMPLE_RATE, K_DEFAULT_MAX_BLOCK_FRAMES, K_CHANNELS},
          controller_(std::make_unique<RigController>(info_)),
          input_storage_(static_cast<std::size_t>(K_CHANNELS) * K_DEFAULT_MAX_BLOCK_FRAMES, 0.0f)
    {
        publish_routing();
    }

    RigAdapter::~RigAdapter() = default;

    RigStateStatus RigAdapter::prepare(double sample_rate, std::uint32_t max_block_frames,
                                       std::string& error_out)
    {
        error_out.clear();
        if (!(sample_rate > 0.0) || max_block_frames == 0)
        {
            error_out = "RigAdapter: the sample rate and the block size must be above zero";
            return RigStateStatus::Refused;
        }
        if (sample_rate == info_.sample_rate && max_block_frames == info_.max_block)
        {
            return RigStateStatus::Ok;
        }

        std::string json_text;
        if (save_state(json_text, error_out) != RigStateStatus::Ok)
        {
            return RigStateStatus::Refused;
        }

        // A RigController keeps the format it was built at; see applications/plugin/README.md.
        const PrepareInfo info{sample_rate, max_block_frames, K_CHANNELS};
        std::unique_ptr<RigController> rebuilt;
        try
        {
            rebuilt = std::make_unique<RigController>(info);
            rebuilt->load_rig_from_json(json_text);
            input_storage_.assign(static_cast<std::size_t>(K_CHANNELS) * max_block_frames, 0.0f);
        }
        catch (const std::exception& error)
        {
            error_out = error.what();
            return RigStateStatus::Refused;
        }

        controller_ = std::move(rebuilt);
        info_ = info;
        pending_count_ = 0;
        publish_routing();
        return RigStateStatus::Ok;
    }

    RigStateStatus RigAdapter::load_state(const std::string& json_text, std::string& error_out)
    {
        error_out.clear();
        reclaim();
        try
        {
            // A refused preset must leave the playing rig alone; see applications/plugin/README.md.
            RigController rehearsal(info_);
            rehearsal.load_rig_from_json(json_text);
        }
        catch (const std::exception& error)
        {
            error_out = error.what();
            return RigStateStatus::Refused;
        }

        RigStateStatus status = RigStateStatus::Ok;
        try
        {
            controller_->load_rig_from_json(json_text);
        }
        catch (const std::exception& error)
        {
            error_out = error.what();
            status = RigStateStatus::Refused;
        }
        publish_routing();
        return status;
    }

    RigStateStatus RigAdapter::save_state(std::string& json_text_out, std::string& error_out)
    {
        error_out.clear();
        reclaim();
        try
        {
            pull_audio_thread_values();
            json_text_out = controller_->save_rig_to_json();
        }
        catch (const std::exception& error)
        {
            error_out = error.what();
            return RigStateStatus::Refused;
        }
        return RigStateStatus::Ok;
    }

    std::uint32_t RigAdapter::latency_frames() const
    {
        const RigGraph* graph = controller_->active_graph();
        return graph != nullptr ? graph->latency_samples() : 0;
    }

    const std::vector<RigParameter>& RigAdapter::parameters() const noexcept
    {
        return parameters_;
    }

    std::optional<float> RigAdapter::parameter_value(std::uint32_t id) const
    {
        ParameterRouting* routing = routing_.active_for_publisher();
        ParameterRouting::Route* route = routing != nullptr ? routing->find(id) : nullptr;
        if (route == nullptr)
        {
            return std::nullopt;
        }
        return route->value.load(std::memory_order_relaxed);
    }

    bool RigAdapter::set_parameter(std::uint32_t id, float value)
    {
        ParameterRouting* routing = routing_.active_for_publisher();
        ParameterRouting::Route* route = routing != nullptr ? routing->find(id) : nullptr;
        if (route == nullptr)
        {
            return false;
        }
        const float clamped = std::clamp(value, route->minimum, route->maximum);
        const Binding& binding = bindings_[static_cast<std::size_t>(route - &routing->at(0))];
        controller_->set_parameter(binding.slot, binding.parameter_id, clamped);
        route->value.store(clamped, std::memory_order_relaxed);
        return true;
    }

    void RigAdapter::reclaim()
    {
        controller_->reclaim();
        routing_.reclaim();
    }

    std::size_t RigAdapter::dropped_parameter_changes() const noexcept
    {
        return dropped_parameter_changes_.load(std::memory_order_relaxed);
    }

    const RigAdapter::Counters& RigAdapter::counters() const noexcept
    {
        return counters_;
    }

    void RigAdapter::add_counters_to(RtCounterReporter& reporter) const
    {
        reporter.add("adapter_format_mismatch", counters_.format_mismatch);
        reporter.add("stale_routing", counters_.stale_routing);
        reporter.add("unknown_parameter", counters_.unknown_parameter);
        reporter.add("dropped_parameter_changes",
                     [this] { return static_cast<std::uint64_t>(dropped_parameter_changes()); });
    }

    void RigAdapter::queue_parameter_for_audio_thread(std::uint32_t id, float value) noexcept
    {
        if (pending_count_ == pending_.size())
        {
            dropped_parameter_changes_.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        pending_[pending_count_] = PendingChange{id, value};
        ++pending_count_;
    }

    void RigAdapter::flush_parameters_for_audio_thread() noexcept
    {
        RigGraph* graph = controller_->graph_slot().acquire_for_audio_thread();
        apply_pending(graph, routing_.acquire_for_audio_thread());
    }

    void RigAdapter::process(const float* const* inputs, float* const* outputs,
                             std::uint32_t channels, std::uint32_t frames) noexcept
    {
        RigGraph* graph = controller_->graph_slot().acquire_for_audio_thread();
        apply_pending(graph, routing_.acquire_for_audio_thread());

        if (channels != K_CHANNELS || frames > info_.max_block)
        {
            counters_.format_mismatch.note(frames);
            clear_outputs(outputs, channels, frames);
            return;
        }

        // The inputs are copied first, so a host that renders in place loses nothing.
        const float* rig_inputs[K_CHANNELS];
        for (std::uint32_t channel = 0; channel < K_CHANNELS; ++channel)
        {
            float* copy =
                input_storage_.data() + static_cast<std::size_t>(channel) * info_.max_block;
            std::memcpy(copy, inputs[channel], frames * sizeof(float));
            rig_inputs[channel] = copy;
        }

        if (graph == nullptr || graph->is_empty())
        {
            for (std::uint32_t channel = 0; channel < K_CHANNELS; ++channel)
            {
                std::memcpy(outputs[channel], rig_inputs[channel], frames * sizeof(float));
            }
            return;
        }

        ProcessContext context{outputs, rig_inputs, frames, 0.0};
        graph->process(context);
    }

    void RigAdapter::publish_routing()
    {
        const std::vector<RigController::SlotId> order = controller_->slot_order();
        const std::size_t slots = std::min<std::size_t>(order.size(), K_PARAMETER_ID_SPAN);
        const RigGraph* graph = controller_->active_graph();
        const std::vector<NodeHandle> handles = handles_of_live_slots(*controller_, graph, order);

        std::vector<const NodeDescriptor*> descriptors(slots, nullptr);
        std::vector<std::uint32_t> parameters_per_slot(slots, 0);
        for (std::size_t position = 0; position < slots; ++position)
        {
            descriptors[position] = find_descriptor(controller_->descriptor_id(order[position]));
            parameters_per_slot[position] = addressable_parameters(descriptors[position]);
        }

        auto routing = std::make_unique<ParameterRouting>(graph, parameters_per_slot);
        std::vector<RigParameter> parameters;
        std::vector<Binding> bindings;
        parameters.reserve(routing->size());
        bindings.reserve(routing->size());

        std::size_t live = 0;
        for (std::size_t position = 0; position < slots; ++position)
        {
            const RigController::SlotId slot = order[position];
            const bool is_live = !controller_->is_bypassed(slot);
            const bool has_node = is_live && live < handles.size();
            const NodeHandle node = has_node ? handles[live] : 0;
            live += is_live ? 1 : 0;

            for (std::uint32_t index = 0; index < parameters_per_slot[position]; ++index)
            {
                const NodeDescriptor& descriptor = *descriptors[position];
                const ParameterDescriptor& source = descriptor.parameters[index];

                RigParameter parameter;
                parameter.id = rig_parameter_id(static_cast<std::uint32_t>(position), index);
                parameter.name = source.label;
                parameter.module = std::to_string(position + 1) + " " + descriptor.name;
                parameter.unit = source.unit != nullptr ? source.unit : "";
                parameter.minimum = source.min;
                parameter.maximum = source.max;
                parameter.default_value = source.default_value;
                parameter.is_stepped =
                    source.kind == ParamKind::Boolean || source.kind == ParamKind::Choice;

                ParameterRouting::Route& route = routing->at(parameters.size());
                route.value.store(controller_->get_parameter(slot, source.id),
                                  std::memory_order_relaxed);
                route.minimum = source.min;
                route.maximum = source.max;
                route.node = node;
                route.index = static_cast<std::uint16_t>(index);
                route.has_node = has_node;

                parameters.push_back(std::move(parameter));
                bindings.push_back(Binding{slot, source.id});
            }
        }

        parameters_ = std::move(parameters);
        bindings_ = std::move(bindings);
        routing_.publish(std::move(routing));
    }

    void RigAdapter::pull_audio_thread_values()
    {
        ParameterRouting* routing = routing_.active_for_publisher();
        if (routing == nullptr)
        {
            return;
        }
        for (std::size_t position = 0; position < bindings_.size(); ++position)
        {
            const Binding& binding = bindings_[position];
            const float value = routing->at(position).value.load(std::memory_order_relaxed);
            if (value != controller_->get_parameter(binding.slot, binding.parameter_id))
            {
                controller_->set_parameter(binding.slot, binding.parameter_id, value);
            }
        }
    }

    void RigAdapter::apply_pending(RigGraph* graph, ParameterRouting* routing) noexcept
    {
        const std::size_t count = pending_count_;
        pending_count_ = 0;

        // A routing published for another graph names that graph's handles; its block is skipped.
        if (routing == nullptr || routing->graph() != graph)
        {
            counters_.stale_routing.increment(count);
            return;
        }
        for (std::size_t entry = 0; entry < count; ++entry)
        {
            ParameterRouting::Route* route = routing->find(pending_[entry].id);
            if (route == nullptr)
            {
                counters_.unknown_parameter.increment();
                continue;
            }
            const float value = std::clamp(pending_[entry].value, route->minimum, route->maximum);
            route->value.store(value, std::memory_order_relaxed);
            AudioNode* node = route->has_node ? graph->node(route->node) : nullptr;
            if (node != nullptr)
            {
                node->set_parameter_for_audio_thread(route->index, value);
            }
        }
    }
} // namespace SushiDSP::Plugin
