/**************************************************************************/
/* cabinet_host.hpp                                                       */
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
 * @file cabinet_host.hpp
 * @brief Declares the interface of a node that carries a cabinet impulse response.
 * @author Mustafa Garip
 */

#pragma once

#include <SushiDSP/foundation/io/ir.hpp>

namespace SushiDSP
{
    /// @brief A node that can carry a cabinet impulse response in its signal path. A host
    /// discovers it with dynamic_cast<CabinetHost*> on an AudioNode, so no host names a
    /// concrete amplifier type to load a cabinet.
    struct CabinetHost
    {
            virtual ~CabinetHost() = default;

            /// @brief Installs a new cabinet impulse response and hands it to the audio thread as one
            /// atomic pointer swap. Control thread only.
            /// @param impulse_response The IR and the rate it was captured at; its sample_rate must
            /// equal the rate this node was prepared at (modules/foundation/core/README.md).
            /// @return True if the cabinet was installed; false if the IR is empty, the rates disagree,
            /// or prepare() has not run yet, and then the active cabinet stays in place.
            virtual bool set_cabinet(Ir impulse_response) = 0;

            /// @brief Frees every cabinet the audio thread has provably moved past.
            /// @pre Called on the control thread, never concurrently with process(); call it
            /// periodically (e.g. once per GUI frame) after any set_cabinet().
            virtual void reclaim_cabinets() = 0;

            /// @brief Whether a cabinet convolution is currently in the signal path.
            /// @return True once a cabinet has been installed by the constructor or set_cabinet().
            virtual bool has_cabinet() const = 0;
    };
} // namespace SushiDSP
