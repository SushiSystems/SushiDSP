/****************************************************************/
/* FileDropQueue.cpp                                            */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file FileDropQueue.cpp
 * @brief Defines the queue that carries GLFW's dropped file paths into the GUI frame.
 * @author Mustafa Garip & Sushi Systems
 */

#include "FileDropQueue.hpp"

#include <utility>

namespace SushiDSP::host
{
    void FileDropQueue::push(std::string path, double screen_x, double screen_y)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pending_.push_back(FileDrop{std::move(path), screen_x, screen_y});
    }

    void FileDropQueue::drain(std::vector<FileDrop>& out)
    {
        std::vector<FileDrop> taken;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            taken.swap(pending_);
        }
        out.insert(out.end(), std::make_move_iterator(taken.begin()), std::make_move_iterator(taken.end()));
    }

    bool FileDropQueue::empty() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return pending_.empty();
    }
}
