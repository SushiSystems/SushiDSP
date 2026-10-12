/**************************************************************************/
/* file_drop_queue.cpp                                                    */
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
 * @file file_drop_queue.cpp
 * @brief Defines the queue that carries GLFW's dropped file paths into the GUI frame.
 * @author Mustafa Garip & Sushi Systems
 */

#include "file_drop_queue.hpp"

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
        out.insert(out.end(), std::make_move_iterator(taken.begin()),
                   std::make_move_iterator(taken.end()));
    }

    bool FileDropQueue::empty() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return pending_.empty();
    }
} // namespace SushiDSP::host
