/****************************************************************/
/* HrtfSetLoader.hpp                                            */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file HrtfSetLoader.hpp
 * @brief Reads the flat asset convert_hrtf.py writes into an HrtfSet.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <string>

#include "SushiDSP/room/HrtfSet.hpp"

namespace SushiDSP::room
{
    /// @brief Reads the flat asset convert_hrtf.py writes. Control thread; the only file this
    /// module opens. Spec §9.2 and §9.3.
    class HrtfSetLoader
    {
    public:
        /// @brief Reads path into set.
        /// @param error_out Filled with the reason on failure: missing, bad magic, wrong version,
        /// a rate mismatch, or a truncated file, each naming the path.
        /// @return False and leaves set untouched on any failure.
        static bool load(const std::string& path, double expected_sample_rate, HrtfSet& set,
                         std::string& error_out);
    };
}
