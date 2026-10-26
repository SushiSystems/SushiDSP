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

#include <cstdint>

namespace SushiDSP
{
    /// @brief Names how CabinetHost::set_cabinet ended.
    enum class CabinetStatus : std::uint8_t
    {
        Installed,            /**< The cabinet is in the signal path. */
        EmptyImpulseResponse, /**< The impulse response holds no samples; nothing changed. */
        NotPrepared,          /**< The node has not been prepared yet; nothing changed. */
        SampleRateMismatch    /**< The impulse response's rate differs from the node's. */
    };

    /// @brief Returns a sentence fragment naming why @p status refused a cabinet, or "installed".
    [[nodiscard]] inline const char* cabinet_status_text(CabinetStatus status) noexcept
    {
        switch (status)
        {
        case CabinetStatus::Installed:
            return "installed";
        case CabinetStatus::EmptyImpulseResponse:
            return "the impulse response is empty";
        case CabinetStatus::NotPrepared:
            return "the node is not prepared yet";
        case CabinetStatus::SampleRateMismatch:
            return "the impulse response's sample rate differs from the node's";
        }
        return "unknown";
    }

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
            /// @return Installed when the cabinet went in; otherwise the reason it was refused (an
            /// empty IR, rates that disagree, or no prepare() yet), and then the active cabinet
            /// stays in place.
            [[nodiscard]] virtual CabinetStatus set_cabinet(Ir impulse_response) = 0;

            /// @brief Frees every cabinet the audio thread has provably moved past.
            /// @pre Called on the control thread, never concurrently with process(); call it
            /// periodically (e.g. once per GUI frame) after any set_cabinet().
            virtual void reclaim_cabinets() = 0;

            /// @brief Whether a cabinet convolution is currently in the signal path.
            /// @return True once a cabinet has been installed by the constructor or set_cabinet().
            virtual bool has_cabinet() const = 0;
    };
} // namespace SushiDSP
