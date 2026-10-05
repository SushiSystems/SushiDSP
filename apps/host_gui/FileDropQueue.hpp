/****************************************************************/
/* FileDropQueue.hpp                                            */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file FileDropQueue.hpp
 * @brief Declares the queue that carries GLFW's dropped file paths into the GUI frame.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <mutex>
#include <string>
#include <vector>

namespace SushiDSP::host
{
    /// @brief One path dropped at one screen position, in the units GLFW's drop callback reports them.
    struct FileDrop
    {
        std::string path;
        double      screen_x;
        double      screen_y;
    };

    /// @brief Carries paths from GLFW's drop callback, which fires outside the ImGui frame, into
    /// the next frame that can route them. Spec §7. Guarded by a std::mutex, which is correct
    /// here because neither push nor drain runs on the audio thread.
    class FileDropQueue
    {
    public:
        /// @brief Appends one dropped path and the screen position it landed at.
        void push(std::string path, double screen_x, double screen_y);

        /// @brief Moves everything queued into out, in the order pushed, and empties the queue.
        void drain(std::vector<FileDrop>& out);

        /// @brief Reports whether nothing is queued.
        bool empty() const;

    private:
        mutable std::mutex     mutex_;
        std::vector<FileDrop>  pending_;
    };
}
