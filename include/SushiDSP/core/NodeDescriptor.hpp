/****************************************************************/
/* NodeDescriptor.hpp                                           */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file NodeDescriptor.hpp
 * @brief Declares the descriptors a host instantiates and drives a product through.
 * @author Mustafa Garip
 */

#pragma once

#include <memory>

#include "SushiDSP/core/IAudioNode.hpp"
#include "SushiDSP/core/PortLayout.hpp"

namespace SushiDSP
{
    /// @brief How a ParameterDescriptor's value maps to a host automation control.
    enum class ParamKind
    {
        Linear,
        Logarithmic,
        Decibel,
        Boolean,
        Choice
    };

    /// @brief One automatable parameter exposed by a product's NodeDescriptor.
    struct ParameterDescriptor
    {
        const char* id;
        const char* label;
        float       min;
        float       max;
        float       default_value;
        ParamKind   kind;
        const char* unit;
    };

    /// @brief Everything a host needs to instantiate and drive a product, with no knowledge of its internals.
    struct NodeDescriptor
    {
        const char*                id;
        const char*                name;
        const ParameterDescriptor* parameters;
        std::size_t                parameter_count;
        PortLayout                 ports;
        std::unique_ptr<IAudioNode> (*make)(const PrepareInfo&);
    };
}
