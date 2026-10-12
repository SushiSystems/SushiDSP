/**************************************************************************/
/* parameter_change.hpp                                                   */
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
 * @file parameter_change.hpp
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
            NodeHandle node;
            std::uint16_t parameter_index;
            float value;
    };
} // namespace SushiDSP
