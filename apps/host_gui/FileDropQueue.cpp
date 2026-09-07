/**************************************************************************/
/* FileDropQueue.cpp                                                      */
/**************************************************************************/
/*                         This file is part of:                          */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                        https://sushisystems.io                         */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License");        */
/* you may not use this file except in compliance with the License.       */
/* You may obtain a copy of the License at                                */
/*                                                                        */
/*     http://www.apache.org/licenses/LICENSE-2.0                         */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

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
