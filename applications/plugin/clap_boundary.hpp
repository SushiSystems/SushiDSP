/**************************************************************************/
/* clap_boundary.hpp                                                      */
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
 * @file clap_boundary.hpp
 * @brief Declares the guard that keeps a C++ exception from crossing the CLAP C interface.
 * @author Mustafa Garip
 */

#pragma once

#include <utility>

namespace SushiDSP::Plugin
{
    /// @brief Runs a call and turns any exception it throws into the callback's failure value.
    /// @param fallback The value the C callback returns when the call throws.
    /// @param call The work of one control-thread callback.
    /// @return The call's own result, or fallback.
    template <typename Result, typename Call> Result guarded(Result fallback, Call&& call) noexcept
    {
        try
        {
            return std::forward<Call>(call)();
        }
        catch (...)
        {
            return fallback;
        }
    }
} // namespace SushiDSP::Plugin
