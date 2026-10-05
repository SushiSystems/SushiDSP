/****************************************************************/
/* ICabinetHost.hpp                                             */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

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

        /// @brief Installs a new cabinet impulse response. Control thread only: the convolver is
        /// built here in full and handed to the audio thread as one atomic pointer swap, so
        /// process() never sees a partially built cabinet and never blocks.
        /// @param impulse_response The IR and the rate it was captured at; its sample_rate must
        /// equal the rate this node was prepared at. The samples are retained so a later
        /// prepare() rebuilds this cabinet, and they are never resampled, so a prepare() at a
        /// different rate needs a fresh set_cabinet().
        /// @return True if the cabinet was installed; false if the IR is empty, if the rates
        /// disagree, or if prepare() has not run yet - in every failing case nothing is installed
        /// and nothing is retained, and the previously active cabinet stays in place.
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
