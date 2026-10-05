/****************************************************************/
/* ICabinetHost.hpp                                             */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file ICabinetHost.hpp
 * @brief Declares the interface of a node that carries a cabinet impulse response.
 * @author Mustafa Garip
 */

#pragma once

#include "SushiDSP/io/Ir.hpp"

namespace SushiDSP
{
    /// @brief A node that can carry a cabinet impulse response in its signal path. A host
    /// discovers it with dynamic_cast<ICabinetHost*> on an IAudioNode, so no host names a
    /// concrete amplifier type to load a cabinet.
    struct ICabinetHost
    {
        virtual ~ICabinetHost() = default;

        /// @brief Installs a new cabinet impulse response and hands it to the audio thread as one
        /// atomic pointer swap. Control thread only.
        /// @param impulse_response The IR and the rate it was captured at; its sample_rate must
        /// equal the rate this node was prepared at (include/SushiDSP/README.md).
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
}
