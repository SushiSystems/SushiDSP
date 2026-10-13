/**************************************************************************/
/* rig_parameter.hpp                                                      */
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
 * @file rig_parameter.hpp
 * @brief Declares one rig parameter as a plugin shell lists it, and the id that names it.
 * @author Mustafa Garip
 */

#pragma once

#include <cstdint>
#include <string>

namespace SushiDSP::Plugin
{
    /// @brief Counts the bits of a parameter id that hold the parameter's index in its product.
    constexpr std::uint32_t K_PARAMETER_INDEX_BITS = 16;

    /// @brief Bounds both halves of a parameter id: slot positions and parameters per slot.
    constexpr std::uint32_t K_PARAMETER_ID_SPAN = 1u << K_PARAMETER_INDEX_BITS;

    /// @brief Builds the id of one parameter of one slot.
    /// @param slot_position The slot's place in the rig's order, below K_PARAMETER_ID_SPAN.
    /// @param parameter_index The parameter's index in its product, below K_PARAMETER_ID_SPAN.
    /// @return The id, the same for the same preset on every load.
    constexpr std::uint32_t rig_parameter_id(std::uint32_t slot_position,
                                             std::uint32_t parameter_index) noexcept
    {
        return (slot_position << K_PARAMETER_INDEX_BITS) | parameter_index;
    }

    /// @brief Reads the slot position out of a parameter id.
    constexpr std::uint32_t rig_parameter_slot(std::uint32_t id) noexcept
    {
        return id >> K_PARAMETER_INDEX_BITS;
    }

    /// @brief Reads the parameter's index in its product out of a parameter id.
    constexpr std::uint32_t rig_parameter_index(std::uint32_t id) noexcept
    {
        return id & (K_PARAMETER_ID_SPAN - 1u);
    }

    /// @brief Describes one parameter of one slot of the rig, in the terms a shell exposes.
    struct RigParameter
    {
            std::uint32_t id = 0;       ///< Holds the id rig_parameter_id() builds.
            std::string name;           ///< Holds the label the product gives the parameter.
            std::string module;         ///< Holds the slot's place and product, as "2 JCM800".
            std::string unit;           ///< Holds the unit, or "" for a bare number.
            float minimum = 0.0f;       ///< Holds the lowest value the product accepts.
            float maximum = 0.0f;       ///< Holds the highest value the product accepts.
            float default_value = 0.0f; ///< Holds the value a fresh slot starts at.
            bool is_stepped = false;    ///< Reports a switch or a choice, which takes whole steps.
    };
} // namespace SushiDSP::Plugin
