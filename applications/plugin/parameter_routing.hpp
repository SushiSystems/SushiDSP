/**************************************************************************/
/* parameter_routing.hpp                                                  */
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
 * @file parameter_routing.hpp
 * @brief Declares the table the audio thread reads to reach a rig parameter's node.
 * @author Mustafa Garip
 */

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <SushiDSP/foundation/core/rig_graph.hpp>

namespace SushiDSP::Plugin
{
    /// @brief Maps every parameter id of one published rig onto the node and index that take it,
    /// and holds each parameter's current value where both threads can read it. The table's shape
    /// is fixed at construction, so a lookup allocates nothing and locks nothing.
    /// @see applications/plugin/README.md, "Parameters on two threads"
    class ParameterRouting final
    {
        public:
            /// @brief Holds one parameter's value and its way to the node that renders it.
            struct Route
            {
                    std::atomic<float> value{0.0f}; ///< Holds the value last set on either thread.
                    float minimum = 0.0f;           ///< Holds the lowest value the node accepts.
                    float maximum = 0.0f;           ///< Holds the highest value the node accepts.
                    NodeHandle node = 0;            ///< Names the node in graph(), when has_node.
                    std::uint16_t index = 0;        ///< Holds the parameter's index in its node.
                    bool has_node = false;          ///< Reports that graph() holds a node for it.
            };

            /// @brief Sizes the table for one rig.
            /// @param graph The published graph the routes' handles name; may be null.
            /// @param parameters_per_slot The parameter count of each slot, in the rig's order.
            ParameterRouting(const RigGraph* graph,
                             const std::vector<std::uint32_t>& parameters_per_slot);

            ParameterRouting(const ParameterRouting&) = delete;
            ParameterRouting& operator=(const ParameterRouting&) = delete;
            ParameterRouting(ParameterRouting&&) = delete;
            ParameterRouting& operator=(ParameterRouting&&) = delete;
            ~ParameterRouting() = default;

            /// @brief Returns the graph the routes' handles are valid in. Real-time safe.
            [[nodiscard]] const RigGraph* graph() const noexcept;

            /// @brief Counts the routes. Real-time safe.
            [[nodiscard]] std::size_t size() const noexcept;

            /// @brief Finds the route a parameter id names. Real-time safe.
            /// @param id An id rig_parameter_id() built.
            /// @return The route, or nullptr when the rig holds no such parameter.
            [[nodiscard]] Route* find(std::uint32_t id) noexcept;

            /// @brief Returns a route by its place in the table, which is the rig's listing order.
            /// @param position A place below size().
            [[nodiscard]] Route& at(std::size_t position) noexcept;

        private:
            /// @brief Names the graph the routes' handles are valid in.
            const RigGraph* graph_;

            /// @brief Holds the first route of each slot, with one closing entry equal to size().
            std::vector<std::size_t> slot_offsets_;

            /// @brief Holds every route, the slots in rig order and each slot's in product order.
            std::vector<Route> routes_;
    };
} // namespace SushiDSP::Plugin
