/**************************************************************************/
/* cascade_filter.hpp                                                     */
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
 * @file cascade_filter.hpp
 * @brief The fourth-order output filter the late tail's cascades run through.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <array>
#include <cstddef>

#include "SushiDSP/math/biquad.hpp"
#include "SushiDSP/room/fdn_design.hpp"

namespace SushiDSP::Room
{
    /// @brief Runs one fourth-order filter of an FdnDesign as its two sections in order. Audio
    /// thread.
    struct CascadeFilter
    {
            /// @brief Stores the two sections, run first to last.
            std::array<Biquad, room::FdnDesign::K_OUTPUT_SECTIONS> sections;

            /// @brief Advances both sections by one sample.
            /// @param input The sample entering the first section.
            /// @return The second section's output.
            float process_sample(float input) noexcept
            {
                for (Biquad& section : sections)
                {
                    input = section.process_sample(input);
                }
                return input;
            }

            /// @brief Copies one design cascade into the two sections; their state is kept.
            /// @param cascade The two sections' b0, b1, b2, a1, a2.
            void load(const room::FdnDesign::Cascade& cascade) noexcept
            {
                for (std::size_t index = 0; index < room::FdnDesign::K_OUTPUT_SECTIONS; ++index)
                {
                    const room::FdnDesign::Section& section = cascade[index];
                    sections[index].set_coefficients(section[0], section[1], section[2],
                                                     section[3], section[4]);
                }
            }

            /// @brief Clears both sections' state.
            void reset() noexcept
            {
                for (Biquad& section : sections)
                {
                    section.reset();
                }
            }
    };
} // namespace SushiDSP::Room
