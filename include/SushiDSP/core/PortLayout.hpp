/****************************************************************/
/* PortLayout.hpp                                               */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file PortLayout.hpp
 * @brief Declares the input and output channel counts of an audio node.
 * @author Mustafa Garip
 */

#pragma once

#include <cstdint>

namespace SushiDSP
{
    /// @brief Describes the fixed input/output channel counts of an IAudioNode.
    struct PortLayout
    {
        std::uint32_t input_channels;
        std::uint32_t output_channels;
    };
}
