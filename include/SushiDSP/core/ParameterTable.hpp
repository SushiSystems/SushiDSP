/**************************************************************************/
/* ParameterTable.hpp                                                     */
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
 * @file ParameterTable.hpp
 * @brief One row per parameter, feeding a node's descriptor and both its dispatches.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "SushiDSP/core/NodeDescriptor.hpp"

namespace SushiDSP
{
    /// @brief One parameter: what a host sees, how it is written and how it is read.
    ///
    /// The two function pointers take the node rather than binding to a member, so a row can
    /// reach a private field through a lambda written inside the node's own scope. That is what
    /// keeps a node from having to expose a public reader for every knob it owns.
    template <typename Node>
    struct ParameterBinding
    {
        ParameterDescriptor descriptor;    ///< The row a host reads: id, label, range, unit.
        void (*write)(Node&, float);       ///< Applies a value to the node.
        float (*read)(const Node&);        ///< Returns what the node currently holds.
    };

    /// @brief A node's whole parameter surface, in index order.
    ///
    /// The order is a contract: a host addresses parameters by index and a saved preset reads
    /// them back by index, so rows are appended, never reordered or removed. Holding the
    /// descriptor and the two accessors in one row is what stops the three from drifting apart.
    template <typename Node, std::size_t kCount>
    class ParameterTable
    {
    public:
        /// @param rows The node's parameters, in the order a host addresses them.
        explicit ParameterTable(const std::array<ParameterBinding<Node>, kCount>& rows)
            : rows_(rows)
        {
            for (std::size_t i = 0; i < kCount; ++i)
            {
                descriptors_[i] = rows_[i].descriptor;
            }
        }

        /// @brief Writes a parameter found by its id. Control thread only: the lookup compares
        /// strings.
        /// @param node The node to write to.
        /// @param id The parameter's id; an unrecognised id is a no-op.
        /// @param value The value to write.
        void write(Node& node, const char* id, float value) const
        {
            const std::size_t index = index_of(id);
            if (index < kCount)
            {
                rows_[index].write(node, value);
            }
        }

        /// @brief Writes a parameter by index. Audio-thread safe: an index is a subscript.
        /// @param node The node to write to.
        /// @param index Index into the table; out of range is a no-op.
        /// @param value The value to write.
        void write(Node& node, std::uint16_t index, float value) const
        {
            if (index < kCount)
            {
                rows_[index].write(node, value);
            }
        }

        /// @brief Reads a parameter found by its id. Control thread only.
        /// @param node The node to read from.
        /// @param id The parameter's id.
        /// @return What the node holds, or 0 for an unrecognised id.
        float read(const Node& node, const char* id) const
        {
            const std::size_t index = index_of(id);
            return index < kCount ? rows_[index].read(node) : 0.0f;
        }

        /// @brief The descriptor rows, contiguous, for a NodeDescriptor to point at.
        /// @return The first of count() descriptors.
        const ParameterDescriptor* descriptors() const noexcept { return descriptors_.data(); }

        /// @brief How many parameters the table holds.
        /// @return The row count, which is also the count a NodeDescriptor declares.
        std::uint16_t count() const noexcept { return static_cast<std::uint16_t>(kCount); }

    private:
        /// @brief Finds a row by id.
        /// @param id The id to look for.
        /// @return Its index, or kCount when no row carries that id.
        std::size_t index_of(const char* id) const
        {
            for (std::size_t i = 0; i < kCount; ++i)
            {
                if (std::strcmp(rows_[i].descriptor.id, id) == 0)
                {
                    return i;
                }
            }
            return kCount;
        }

        std::array<ParameterBinding<Node>, kCount> rows_;
        std::array<ParameterDescriptor, kCount> descriptors_{};
    };
}
