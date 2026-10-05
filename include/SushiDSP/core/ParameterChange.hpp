/****************************************************************/
/* ParameterChange.hpp                                          */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file ParameterChange.hpp
 * @brief Declares the node handle and the parameter write sent to the audio thread.
 * @author Mustafa Garip
 */

#pragma once

#include <cstdint>

namespace SushiDSP
{
    /// @brief Identifies one node inside a RigGraph, as returned by RigGraph::add_node().
    using NodeHandle = std::uint32_t;

    /// @brief One parameter write travelling from the control thread to the audio thread.
    struct ParameterChange
    {
        NodeHandle    node;
        std::uint16_t parameter_index;
        float         value;
    };
}
